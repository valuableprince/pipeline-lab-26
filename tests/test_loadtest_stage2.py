"""LOADTEST-03: test orchestration without connecting to a real game server."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import textwrap
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "loadtest_stage2", ROOT / "tools/loadtest_stage2.py"
)
stage = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = stage
spec.loader.exec_module(stage)


class ConfigTests(unittest.TestCase):
    def args(self, *extra):
        return stage.parse_args([
            "--server", "127.0.0.1:7777",
            "--confirm-target", "127.0.0.1:7777",
            "--dry-run", *extra,
        ])

    def test_safe_dry_run_does_not_spawn_or_write(self):
        args = self.args("--count", "5")
        self.assertEqual(stage.validate(args), ("127.0.0.1", 7777))
        with patch.object(stage.subprocess, "Popen") as popen:
            self.assertEqual(stage.run(args), 0)
            popen.assert_not_called()

    def test_reject_unapproved_remote_and_bad_confirmation(self):
        a = self.args()
        a.server = "example.org:7777"
        a.confirm_target = a.server
        with self.assertRaisesRegex(ValueError, "remote"):
            stage.validate(a)
        a.remote_approved = True
        stage.validate(a)
        a.confirm_target = "other.org:7777"
        with self.assertRaisesRegex(ValueError, "match"):
            stage.validate(a)

    def test_capped_load_and_bad_targets(self):
        a = self.args()
        for count in (0, 51, 999):
            a.count = count
            with self.assertRaises(ValueError):
                stage.validate(a)
        a.count = 5
        for seconds in (0, 601):
            a.seconds = seconds
            with self.assertRaises(ValueError):
                stage.validate(a)
        a.seconds = 90
        for rate in (-1.0, 0, 2.01):
            a.rate = rate
            with self.assertRaises(ValueError):
                stage.validate(a)
        for target in ("ftp://localhost:7777", "a:b", "a:70000", "a:1/next",
                       "abc:def:7777", "bad host:1234", ""):
            with self.subTest(target=target):
                with self.assertRaises(ValueError):
                    stage.parse_target(target)

    def test_auth_template_and_no_command_flood(self):
        secret = 'abC12345"\\'
        lua = stage.render_script(secret, "register", "commands")
        self.assertIn('sampSendDialogResponse(dialogId, 1, 0, secret)', lua)
        self.assertIn('local secret = '+json.dumps(secret), lua)
        self.assertIn('wait(30000)', lua)
        self.assertIn('sampSendRpc(50,', lua)
        self.assertIn('dialogId == 6104', lua)
        self.assertIn('LT_STATE:spawned', lua)
        self.assertNotIn('os.execute', lua)
        with self.assertRaises(ValueError):
            stage.render_script("bad", "login", "idle")


class ReportingTests(unittest.TestCase):
    def test_revisioned_reports(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            date = stage.datetime(2026, 10, 26)
            first = stage.unique_report_dir(root, date)
            second = stage.unique_report_dir(root, date)
            self.assertEqual(first.name, "load_net_01_26.10.2026")
            self.assertEqual(second.name, "load_net_02_26.10.2026")

    def test_log_markers_must_exist_in_transcript(self):
        states = stage.classify_log(
            "INFO raknet transport connected\njoined game\n"
            "LT_STATE:authenticated\nLT_STATE:spawned\n"
        )
        self.assertTrue(states["spawned"])
        self.assertTrue(states["game_joined"])
        self.assertTrue(states["authenticated"])
        self.assertFalse(states["failed"])
        self.assertFalse(stage.classify_log("")["spawned"])

    def test_snapshot_reads_real_markers_not_requested_count(self):
        with tempfile.TemporaryDirectory() as tmp:
            log = Path(tmp) / "client.log"
            log.write_text("joined game\nLT_STATE:spawned\n", encoding="utf-8")
            class Fake:
                def poll(self):
                    return None
            row = stage.snapshot([{"log": log, "process": Fake()}], 5)
            self.assertEqual(row["game_joined"], 1)
            self.assertEqual(row["spawned"], 1)
            self.assertEqual(row["authenticated"], 0)
            self.assertEqual(row["alive"], 1)


if __name__ == "__main__":
    unittest.main()
