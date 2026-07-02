#!/usr/bin/env python3
"""GTK3 candidate popover for VietKey IBus engine."""

import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk

class CandidateWindow:
    MAX_CANDIDATES = 9

    def __init__(self, engine):
        self.engine = engine
        self.window = None
        self.candidates = []

    def show(self, candidates, x, y):
        self.candidates = candidates[:self.MAX_CANDIDATES]
        if not self.window:
            self._create_window()
        self._update_candidates()
        self.window.move(x, y)
        self.window.show_all()

    def hide(self):
        if self.window:
            self.window.hide()

    def _create_window(self):
        self.window = Gtk.Window(type=Gtk.WindowType.POPUP)
        self.window.set_decorated(False)
        self.window.set_skip_taskbar_hint(True)
        self.window.set_skip_pager_hint(True)
        self.window.set_type_hint(Gdk.WindowTypeHint.POPUP_MENU)

        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=4)
        box.set_margin_start(4)
        box.set_margin_end(4)
        box.set_margin_top(2)
        box.set_margin_bottom(2)
        self.window.add(box)
        self.box = box

    def _update_candidates(self):
        for child in self.box.get_children():
            self.box.remove(child)
        for i, cand in enumerate(self.candidates):
            label = Gtk.Label(label=f"{i+1}. {cand}")
            label.set_margin_start(4)
            label.set_margin_end(4)
            self.box.pack_start(label, False, False, 0)
        self.box.show_all()

    def select(self, index):
        if 0 <= index < len(self.candidates):
            text = self.candidates[index]
            self.engine.commit_text(
                __import__("ibus").Text.new_from_string(text)
            )
            self.hide()
            return True
        return False
