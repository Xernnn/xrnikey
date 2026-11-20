"""Tests for xk-setup; run with python3 cli-tools/test_xk_setup.py.

Only --dry-run is used, with every path it reads or writes under a temporary
directory, so nothing on the machine is looked at beyond running processes
and installed programs, and nothing is changed.
"""
import os
import subprocess
import tempfile
import unittest

SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "xk-setup.sh")
PLUGIN = "platforminputcontexts/libfcitx5platforminputcontextplugin.so"


class XkSetupTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.plugins = os.path.join(self.tmp.name, "qt-plugins")
        os.makedirs(os.path.join(self.plugins, "platforminputcontexts"))
        self.env = {k: v for k, v in os.environ.items()
                    if k not in ("QT_IM_MODULE", "GTK_IM_MODULE", "XMODIFIERS",
                                 "SDL_IM_MODULE", "DISPLAY")}
        self.env.update(XDG_CONFIG_HOME=os.path.join(self.tmp.name, "config"),
                        XDG_DATA_HOME=os.path.join(self.tmp.name, "data"),
                        WAYLAND_DISPLAY="wayland-test",
                        XK_QT_PLUGIN_DIRS=self.plugins)

    def tearDown(self):
        self.tmp.cleanup()

    def dry_run(self, **env):
        r = subprocess.run(["bash", SCRIPT, "--dry-run"], env=dict(self.env, **env),
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertFalse(os.path.exists(os.path.join(self.tmp.name, "config", "environment.d")))
        return r.stdout, r.stderr

    def offered(self, out):
        lines = [l for l in out.splitlines() if "would: write" in l]
        self.assertEqual(len(lines), 1, out)
        return lines[0]

    def test_offers_qt_im_module_when_fcitx5_qt_is_installed(self):
        open(os.path.join(self.plugins, PLUGIN), "w").close()
        out, _ = self.dry_run()
        self.assertIn("QT_IM_MODULE=fcitx", self.offered(out))

    def test_skips_qt_im_module_without_fcitx5_qt_and_says_why(self):
        out, _ = self.dry_run()
        line = self.offered(out)
        self.assertNotIn("QT_IM_MODULE", line)
        self.assertIn("XMODIFIERS=@im=fcitx", line)
        self.assertIn("QT_IM_MODULE=fcitx not offered", out)
        self.assertIn("fcitx5-qt", out)

    def test_warns_when_qt_im_module_points_at_a_missing_module(self):
        _, err = self.dry_run(QT_IM_MODULE="fcitx")
        self.assertIn("fcitx5-qt", err)


if __name__ == "__main__":
    unittest.main()
