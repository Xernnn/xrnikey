#!/usr/bin/env python3
"""GSettings schema wrapper for Xrnikey IBus engine."""

import os

try:
    from gi.repository import Gio
    HAS_GSETTINGS = True
except ImportError:
    HAS_GSETTINGS = False

SCHEMA_ID = "com.xrnikey"
SCHEMA_PATH = "/com/xrnikey/"

DEFAULTS = {
    "input-mode": "Telex",
    "auto-switch": True,
    "prediction-enabled": True,
    "emoji-enabled": True,
    "emoji-trigger": ":",
    "auto-correct": True,
}

class XrnikeyConfig:
    def __init__(self):
        self._settings = None
        if HAS_GSETTINGS:
            try:
                self._settings = Gio.Settings.new(SCHEMA_ID)
            except Exception:
                pass

    def _get(self, key):
        if self._settings:
            return self._settings.get_value(key).unpack()
        return DEFAULTS.get(key.replace("-", "_"), DEFAULTS.get(key))

    @property
    def input_mode(self):
        return self._get("input-mode")

    @property
    def auto_switch(self):
        return self._get("auto-switch")

    @property
    def prediction_enabled(self):
        return self._get("prediction-enabled")

    @property
    def emoji_enabled(self):
        return self._get("emoji-enabled")

    @property
    def emoji_trigger(self):
        return self._get("emoji-trigger")

    @property
    def auto_correct(self):
        return self._get("auto-correct")
