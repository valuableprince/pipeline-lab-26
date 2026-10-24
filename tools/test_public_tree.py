#!/usr/bin/env python3
"""Synthetic tests for the public-tree allowlist. No private inputs or keys."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

CHECKER = Path(__file__).with_name("verify_public_tree.py").read_text(encoding="utf-8")
SAFE_FILE = "# test fixture\n"
AGE_HEADER = b"age-encryption.org/v1\n"

class PublicTreeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        (self.root / "tools").mkdir()
        (self.root / "tools" / "verify_public_tree.py").write_text(CHECKER)
        self.stage("tools/verify_public_tree.py")

    def stage(self, path):
        subprocess.run(["git", "-C", str(self.root), "add", "-f", path], check=True)

    def check(self):
        return subprocess.run(
            [sys.executable, "tools/verify_public_tree.py"],
            cwd=self.root, capture_output=True, text=True,
        )

    def test_allowlisted_file(self):
        self.assertEqual(self.check().returncode, 0)

    def test_reject_plaintext_file(self):
        (self.root / "example.pwn").write_text(SAFE_FILE)
        self.stage("example.pwn")
        self.assertNotEqual(self.check().returncode, 0)

    def test_reject_unknown_payload(self):
        (self.root / "payload").mkdir()
        (self.root / "payload" / "extra.age").write_bytes(AGE_HEADER + b"x" * 150)
        self.stage("payload/extra.age")
        self.assertNotEqual(self.check().returncode, 0)

    def test_reject_bad_header(self):
        (self.root / "payload").mkdir()
        (self.root / "payload" / "release.age").write_bytes(b"not age" + b"x" * 150)
        self.stage("payload/release.age")
        self.assertNotEqual(self.check().returncode, 0)

    def test_reject_symlink(self):
        (self.root / "tools" / "pack.sh").symlink_to("verify_public_tree.py")
        self.stage("tools/pack.sh")
        self.assertNotEqual(self.check().returncode, 0)

if __name__ == "__main__":
    unittest.main()
