#!/usr/bin/env python3
"""Xrnikey IBus engine — fallback for GNOME users."""

import os
import sys
import ctypes
import ibus
from gi.repository import GObject

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from config import XrnikeyConfig
from candidate_window import CandidateWindow

XK_MODE_TELEX = 0
XK_MODE_VNI = 1

class XKResult(ctypes.Structure):
    # Field order/types must mirror XKResult in core/xrnikey.h exactly —
    # this struct is returned by value across the ctypes boundary.
    _fields_ = [
        ("commit", ctypes.c_char * 64),
        ("preedit", ctypes.c_char * 64),
        ("consumed", ctypes.c_int),
        ("reset", ctypes.c_int),
        ("delete_before", ctypes.c_int),
    ]

class XrnikeyEngine(ibus.Engine):
    __gtype_name__ = "XrnikeyEngine"

    def __init__(self):
        super().__init__()
        self.config = XrnikeyConfig()
        self._load_libxrnikey()
        mode = XK_MODE_VNI if self.config.input_mode == "VNI" else XK_MODE_TELEX
        self.state = self.lib.xk_state_new(mode)
        self.candidates = CandidateWindow(self)
        self.preedit_text = ""

    def _load_libxrnikey(self):
        lib_paths = [
            "/usr/lib/libxrnikey.so",
            "/usr/local/lib/libxrnikey.so",
            os.path.join(os.path.dirname(__file__), "..", "build", "core", "libxrnikey.so"),
        ]
        self.lib = None
        for path in lib_paths:
            if os.path.exists(path):
                self.lib = ctypes.CDLL(path)
                break
        if not self.lib:
            raise RuntimeError("libxrnikey.so not found")

        self.lib.xk_state_new.argtypes = [ctypes.c_int]
        self.lib.xk_state_new.restype = ctypes.c_void_p
        self.lib.xk_state_free.argtypes = [ctypes.c_void_p]
        self.lib.xk_process_key.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32]
        self.lib.xk_process_key.restype = XKResult
        self.lib.xk_state_reset.argtypes = [ctypes.c_void_p]

    def do_process_key_event(self, keyval, keycode, state):
        mods = 0
        if state & ibus.ModifierType.CONTROL_MASK:
            mods |= 0x04
        if state & ibus.ModifierType.SHIFT_MASK:
            mods |= 0x01

        result = self.lib.xk_process_key(self.state, keyval, mods)

        if result.reset:
            self.lib.xk_state_reset(self.state)
            self.update_preedit_text(ibus.Text("", len=0))
            return True

        if result.commit:
            commit_str = result.commit.decode("utf-8")
            self.commit_text(ibus.Text.new_from_string(commit_str))
            self.lib.xk_state_reset(self.state)
            self.update_preedit_text(ibus.Text("", len=0))
            return True

        if result.consumed:
            preedit = result.preedit.decode("utf-8") if result.preedit else ""
            attrs = ibus.AttrList()
            attrs.append(ibus.AttributeUnderline.new(0, len(preedit)))
            text = ibus.Text.new_from_string(preedit)
            text.set_attributes(attrs)
            self.update_preedit_text(text, True)
            return True

        return False

    def do_focus_in(self):
        self.register_properties(self._setup_properties())

    def do_focus_out(self):
        self.lib.xk_state_reset(self.state)
        self.update_preedit_text(ibus.Text("", len=0))

    def do_reset(self):
        self.lib.xk_state_reset(self.state)
        self.update_preedit_text(ibus.Text("", len=0))

    def _setup_properties(self):
        props = ibus.PropList()
        props.append(ibus.Property.new(
            "xrnikey.mode",
            ibus.PROP_TYPE_NORMAL,
            ibus.PROP_STATE_UNCHECKED,
            "Xrnikey",
            self.config.input_mode,
            True, True, 0, None
        ))
        return props

class XrnikeyFactory(ibus.EngineFactory):
    __gtype_name__ = "XrnikeyFactory"

    def __init__(self):
        super().__init__()
        self.engine_type = XrnikeyEngine

    def do_create_engine(self, engine_name):
        if engine_name == "xrnikey":
            return XrnikeyEngine()
        return None

def main():
    ibus.init()
    component = ibus.Component(
        ibus.Component.get_name(),
        ibus.Component.get_description(),
        ibus.Component.get_version(),
        ibus.Component.get_license(),
        ibus.Component.get_author(),
        ibus.Component.get_homepage(),
        ibus.Component.get_textdomain(),
        ""
    )
    factory = XrnikeyFactory()
    component.add_engine("xrnikey", "Xrnikey", "vi", "GPL-3.0")
    component.set_engine_factory(factory)
    return component

if __name__ == "__main__":
    main()
