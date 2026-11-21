"""Tests for xk-macro; run with python3 cli-tools/test_xk_macro.py."""
import os
import subprocess
import sys
import tempfile
import unittest

SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "xk-macro.py")


class XkMacroTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.env = dict(os.environ, XDG_DATA_HOME=self.tmp.name)
        self.file = os.path.join(self.tmp.name, "xrnkey", "macros.txt")

    def tearDown(self):
        self.tmp.cleanup()

    def run_cli(self, *args, ok=True):
        r = subprocess.run([sys.executable, SCRIPT, *args], env=self.env,
                           capture_output=True, text=True)
        self.assertEqual(r.returncode == 0, ok, r.stderr)
        return r.stdout

    def content(self):
        with open(self.file, encoding="utf-8") as f:
            return f.read()

    def test_add_writes_the_file_the_addon_reads(self):
        self.run_cli("add", "ko", "không")
        self.run_cli("add", "vn", "Việt", "Nam")
        self.assertEqual(self.content(), "ko\tkhông\nvn\tViệt Nam\n")
        self.assertEqual(self.run_cli("list"), "ko\tkhông\nvn\tViệt Nam\n")
        self.assertEqual(self.run_cli("path").strip(), self.file)

    def test_add_replaces_an_abbreviation_whatever_its_case(self):
        self.run_cli("add", "ko", "không")
        self.run_cli("add", "KO", "chẳng")
        self.assertEqual(self.content(), "KO\tchẳng\n")

    def test_remove_keeps_comments_and_other_lines(self):
        os.makedirs(os.path.dirname(self.file))
        with open(self.file, "w", encoding="utf-8") as f:
            f.write("# mine\nko\tkhông\ndc\tđược\n")
        self.run_cli("remove", "KO")
        self.assertEqual(self.content(), "# mine\ndc\tđược\n")
        self.run_cli("remove", "nope", ok=False)

    def test_rejects_what_the_file_cannot_hold(self):
        self.run_cli("add", "two words", "x", ok=False)
        self.run_cli("add", "ab", "has\ttab", ok=False)
        self.assertFalse(os.path.exists(self.file))


if __name__ == "__main__":
    unittest.main()
