"""Tests for xk-dict; run with python3 cli-tools/test_xk_dict.py.

Only the commands that need no installed libxrnkey (add, stats), with
XDG_DATA_HOME in a temporary directory.
"""
import os
import subprocess
import sys
import tempfile
import unittest

SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "xk-dict.py")


class XkDictTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.env = dict(os.environ, XDG_DATA_HOME=self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def run_cli(self, *args):
        r = subprocess.run([sys.executable, SCRIPT, *args], env=self.env,
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        return r.stdout

    def test_add_writes_the_file_the_addon_reads(self):
        self.run_cli("add", "từ mới", "9000")
        with open(os.path.join(self.tmp.name, "xrnkey", "user-dict.txt"), encoding="utf-8") as f:
            self.assertEqual(f.read(), "từ mới\t9000\n")

    def test_moves_the_folder_from_before_the_rename(self):
        old = os.path.join(self.tmp.name, "xrnikey")
        os.makedirs(old)
        with open(os.path.join(old, "user-dict.txt"), "w", encoding="utf-8") as f:
            f.write("hạnh phúc\t500\n")
        self.run_cli("add", "từ mới", "9000")
        self.assertFalse(os.path.exists(old))
        with open(os.path.join(self.tmp.name, "xrnkey", "user-dict.txt"), encoding="utf-8") as f:
            self.assertEqual(f.read(), "hạnh phúc\t500\ntừ mới\t9000\n")


if __name__ == "__main__":
    unittest.main()
