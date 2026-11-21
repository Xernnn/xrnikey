"""Tests for xk-setup; run with python3 cli-tools/test_xk_setup.py.

Only --dry-run and --rollback are used, with every path they read or write
under a temporary directory, so nothing on the machine is looked at beyond
running processes and installed programs, and nothing outside it is changed.
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

    def old_setup(self):
        """What xk-setup left before the rename (xrnikey): the environment file
        it wrote, its marker, and a backup of the version before."""
        envd = os.path.join(self.tmp.name, "config", "environment.d")
        backups = os.path.join(self.tmp.name, "data", "xrnikey", "setup-backup")
        os.makedirs(envd)
        os.makedirs(backups)
        for path, text in ((os.path.join(envd, "90-xrnikey.conf"), "XMODIFIERS=@im=fcitx\n"),
                           (os.path.join(envd, "90-xrnikey.conf.xk-setup"), ""),
                           (os.path.join(backups, "90-xrnikey.conf.20261101120000"), "SDL_IM_MODULE=fcitx\n")):
            with open(path, "w") as f:
                f.write(text)
        return envd

    def test_rollback_undoes_what_it_did_before_the_rename(self):
        envd = self.old_setup()
        r = subprocess.run(["bash", SCRIPT, "--rollback"], env=self.env,
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(sorted(os.listdir(envd)), ["90-xrnkey.conf"])
        with open(os.path.join(envd, "90-xrnkey.conf")) as f:
            self.assertEqual(f.read(), "SDL_IM_MODULE=fcitx\n")
        data = os.path.join(self.tmp.name, "data")
        self.assertEqual(os.listdir(data), ["xrnkey"])
        self.assertEqual(os.listdir(os.path.join(data, "xrnkey", "setup-backup")),
                         ["90-xrnkey.conf.20261101120000"])

    def test_dry_run_only_says_it_would_move_the_old_files(self):
        self.old_setup()
        r = subprocess.run(["bash", SCRIPT, "--dry-run"], env=self.env,
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("would: move", r.stdout)
        self.assertTrue(os.path.exists(os.path.join(self.tmp.name, "data", "xrnikey")))
        self.assertEqual(sorted(os.listdir(os.path.join(self.tmp.name, "config", "environment.d"))),
                         ["90-xrnikey.conf", "90-xrnikey.conf.xk-setup"])


if __name__ == "__main__":
    unittest.main()
