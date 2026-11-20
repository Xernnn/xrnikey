"""Tests for xk-diagnose; run with python3 cli-tools/test_xk_diagnose.py.

xk-diagnose only reads (running processes, installed files, fonts); these
tests run it in a temporary directory and check what it writes where.
"""
import os
import socket
import subprocess
import sys
import tempfile
import unittest

SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "xk-diagnose.sh")


class XkDiagnoseTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.cwd = os.path.join(self.tmp.name, "cwd")
        os.makedirs(self.cwd)

    def tearDown(self):
        self.tmp.cleanup()

    def run_cli(self, *args):
        return subprocess.run(["bash", SCRIPT, *args], cwd=self.cwd,
                              capture_output=True, text=True)

    def test_prints_the_report_and_writes_no_file(self):
        r = self.run_cli()
        self.assertIn("Xrnikey Diagnostic Report", r.stdout)
        self.assertIn("Issues found:", r.stdout)
        self.assertEqual(os.listdir(self.cwd), [])

    def test_leaves_out_host_and_user_unless_asked(self):
        out = self.run_cli().stdout
        self.assertNotIn("Hostname:", out)
        self.assertNotIn("User:", out)
        full = self.run_cli("--full").stdout
        self.assertIn("Hostname: " + socket.gethostname(), full)
        self.assertIn("User:", full)

    def test_output_writes_the_report_to_the_path_given(self):
        path = os.path.join(self.tmp.name, "report.txt")
        r = self.run_cli("--output", path)
        with open(path, encoding="utf-8") as f:
            report = f.read()
        self.assertIn("Xrnikey Diagnostic Report", report)
        self.assertNotIn("Hostname:", report)
        self.assertTrue(r.stdout.startswith(report.splitlines()[0]))
        self.assertIn("Report saved to: " + path, r.stdout)
        self.assertEqual(os.listdir(self.cwd), [])

    def test_rejects_unknown_arguments(self):
        self.assertEqual(self.run_cli("--bogus").returncode, 2)
        self.assertEqual(self.run_cli("--output").returncode, 2)


if __name__ == "__main__":
    unittest.main()
