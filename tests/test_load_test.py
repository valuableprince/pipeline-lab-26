"""LOADTEST-01 regression tests for isolated NPC load probe and CSV report."""

import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
LOADTEST = (ROOT / "gamemodes/includes/load_test.inc").read_text(encoding="utf-8")
DOC = (ROOT / "docs/LOAD_TEST.md").read_text(encoding="utf-8")

spec = importlib.util.spec_from_file_location(
    "loadtest_report", ROOT / "tools/loadtest_report.py"
)
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)


class LoadTestIntegration(unittest.TestCase):
    def test_game_lifecycle_and_command(self):
        self.assertIn('#include "load_test.inc"', GAME)
        for expected in (
            "LoadTest_Init();", "LoadTest_Shutdown();",
            "LoadTest_OnDisconnect(playerid);",
        ):
            self.assertIn(expected, GAME)
        self.assertIn("cmd:loadtest(playerid, params[])", COMMANDS)
        self.assertIn("LoadTest_Command(playerid, params);", COMMANDS)

    def test_safety_gate_permissions_budgets_and_cleanup(self):
        for expected in (
            '"load_test.enabled"',
            "if (!fexist(LOADTEST_ENABLE_FLAG))",
            "!Admin_IsFounder(playerid) || !gAdminDuty[playerid]",
            "#define LOADTEST_MAX_NPCS        (100)",
            "#define LOADTEST_BATCH_NPCS      (4)",
            "#define LOADTEST_MAX_SECONDS     (300)",
            "LOADTEST_MIN_TICKRATE",
            "LOADTEST_BAD_SAMPLES",
            "LoadTest_Stop(\"owner_disconnect\")",
            "LoadTest_Stop(\"flag_removed\")",
            "NPC_Destroy(gLoadNPCIds[i]);",
            "KillTimer(gLoadTimer);",
        ):
            self.assertIn(expected, LOADTEST)
        self.assertNotIn("mysql_query", LOADTEST)
        self.assertNotIn("mysql_tquery", LOADTEST)
        self.assertIn("not a real-player", DOC.lower().replace("**", ""))
        self.assertIn("scriptfiles/loadtest.csv", DOC)

    def test_no_global_npc_deletion(self):
        self.assertNotIn("NPC_DestroyAll", LOADTEST)
        self.assertIn("NPC_Create(name)", LOADTEST)
        self.assertIn("NPC_Spawn(npcid)", LOADTEST)
        self.assertIn("NPC_Move(id,", LOADTEST)


class LoadTestReport(unittest.TestCase):
    def _read(self, lines):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "loadtest.csv"
            path.write_text("\n".join(lines) + "\n", encoding="utf-8")
            return report.load_samples(path)

    def test_completed_and_incomplete_runs(self):
        rows = self._read([
            "100,0,0,8,90,90,0,1000,0,start",
            "100,5,8,8,70,65,1200,1000,0,running",
            "100,12,8,8,70,65,1200,1000,0,complete",
            "200,0,0,50,80,80,0,1000,0,start",
            "200,5,20,50,50,40,1400,1000,0,running",
        ])
        summaries = report.summarize(rows)
        self.assertEqual(len(summaries), 2)
        self.assertEqual(summaries[0]["peak_npcs"], 8)
        self.assertEqual(summaries[0]["min_tickrate"], 70)
        self.assertTrue(summaries[0]["target_reached"])
        self.assertEqual(summaries[0]["end_state"], "complete")
        self.assertFalse(summaries[1]["target_reached"])
        self.assertEqual(summaries[1]["end_state"], "incomplete")

    def test_corrupt_and_invalid_rows_rejected(self):
        for line in (
            "100,1,4,2,70,60,900,1000,0,running",
            "100,1,1,3,INVALID,60,900,1000,0,running",
            "100,-1,1,3,70,60,900,1000,0,running",
            "100,1,1,3,70",
        ):
            with self.subTest(line=line):
                with self.assertRaises(ValueError):
                    self._read([line])


if __name__ == "__main__":
    unittest.main()
