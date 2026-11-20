"""Act as a text field talking to fcitx5 over org.fcitx.Fcitx.InputContext1,
the way the fcitx5 GTK and Qt modules do. Run by dbus_session.sh."""
import os
import sys
import time

import gi

gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib  # noqa: E402

CONF = os.environ["DRIVER_CONF"]
IC = "org.fcitx.Fcitx.InputContext1"
CONTROLLER = "org.fcitx.Fcitx.Controller1"
PREEDIT, PASSWORD, FORMATTED, UNFOCUS_COMMIT, SURROUNDING = 1 << 1, 1 << 3, 1 << 4, 1 << 5, 1 << 6
SENSITIVE = 1 << 36
KEYS = {" ": 0x20, "\n": 0xFF0D, "\b": 0xFF08}

conn = Gio.bus_get_sync(Gio.BusType.SESSION, None)
ctx = GLib.MainContext.default()


def call(path, iface, method, sig=None, args=None, ret=None, dest="org.fcitx.Fcitx5"):
    v = conn.call_sync(dest, path, iface, method,
                       GLib.Variant(sig, args) if sig else None,
                       GLib.VariantType(ret) if ret else None, 0, 5000, None)
    return v.unpack() if v else None


def pump():
    """Deliver the signals fcitx5 sent (they precede each method reply)."""
    end = time.time() + 0.03
    while time.time() < end:
        while ctx.pending():
            ctx.iteration(False)
        time.sleep(0.003)


def wait_for_fcitx5():
    for _ in range(100):
        names = call("/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames",
                     ret="(as)", dest="org.freedesktop.DBus")[0]
        if "org.fcitx.Fcitx5" in names:
            return
        time.sleep(0.1)
    sys.exit("fcitx5 never appeared on the bus")


class Field:
    """A text field: applies commits, deletions and unhandled keys, and
    reports its text back as surrounding text."""

    def __init__(self, caps):
        self.text, self.cursor, self.preedit, self.forwarded = "", 0, "", []
        self.path, _ = call("/org/freedesktop/portal/inputmethod", "org.fcitx.Fcitx.InputMethod1",
                            "CreateInputContext", "(a(ss))", ([("program", "dbus-test-app")],),
                            "(oay)")
        self.sub = conn.signal_subscribe(None, IC, None, self.path, None,
                                         Gio.DBusSignalFlags.NONE, self.on_signal)
        call(self.path, IC, "SetCapability", "(t)", (caps,))
        call(self.path, IC, "FocusIn")
        self.report()
        call("/controller", CONTROLLER, "SetCurrentIM", "(s)", ("xrnikey",))
        pump()

    def close(self):
        call(self.path, IC, "DestroyIC")
        conn.signal_unsubscribe(self.sub)

    def insert(self, s):
        self.text = self.text[:self.cursor] + s + self.text[self.cursor:]
        self.cursor += len(s)

    def report(self):
        call(self.path, IC, "SetSurroundingText", "(suu)", (self.text, self.cursor, self.cursor))

    def on_signal(self, _c, _sender, _path, _iface, name, params):
        p = params.unpack()
        if name == "CommitString":
            self.insert(p[0])
            self.report()
        elif name == "DeleteSurroundingText":
            start = self.cursor + p[0]
            self.text = self.text[:start] + self.text[start + p[1]:]
            self.cursor = start
            self.report()
        elif name == "UpdateFormattedPreedit":
            self.preedit = "".join(s for s, _ in p[0])
        elif name == "ForwardKey":
            self.forwarded.append(p)

    def type(self, keys):
        for ch in keys:
            sym = KEYS.get(ch, ord(ch))
            handled = call(self.path, IC, "ProcessKeyEvent", "(uuubu)", (sym, 0, 0, False, 0), "(b)")[0]
            call(self.path, IC, "ProcessKeyEvent", "(uuubu)", (sym, 0, 0, True, 0), "(b)")
            pump()
            if not handled:
                if ch == "\b":
                    if self.cursor:
                        self.text = self.text[:self.cursor - 1] + self.text[self.cursor:]
                        self.cursor -= 1
                else:
                    self.insert(ch)
                self.report()
                pump()


def set_options(text):
    with open(CONF, "w", encoding="utf-8") as fp:
        fp.write(text)
    call("/controller", CONTROLLER, "ReloadAddonConfig", "(s)", ("xrnikey5",))
    pump()


failures = []


def expect(what, got, want):
    print(("ok   " if got == want else "FAIL ") + what + ": " + repr(got)
          + ("" if got == want else "  (want " + repr(want) + ")"))
    if got != want:
        failures.append(what)


wait_for_fcitx5()
f = Field(PREEDIT | FORMATTED | UNFOCUS_COMMIT | SURROUNDING)
f.type("chaof")
expect("the word is shown inline", f.preedit, "chào")
f.type("\n")
expect("one Return commits the word and the newline", f.text, "chào\n")
set_options("InstantCommit=True\n")
f.type("vieetj nam ")
expect("InstantCommit=True applies to the open field after a reload", f.text, "chào\nviệt nam ")
expect("instant mode shows no preedit", f.preedit, "")
set_options("InputMode=VNI\n")
f.type("a1 ")
expect("VNI applies to the open field after a reload", f.text, "chào\nviệt nam á ")
set_options("PredictionEnabled=True\n")
f.type("ch1 ")
expect("a digit never picks a suggestion", f.text, "chào\nviệt nam á ch1 ")
expect("no keys are synthesised", f.forwarded, [])
f.close()

set_options("")
g = Field(PREEDIT | FORMATTED | UNFOCUS_COMMIT | SURROUNDING | SENSITIVE)
g.type("maatj")
expect("a sensitive field gets the keys untouched", g.text, "maatj")
g.close()

sys.exit(1 if failures else 0)
