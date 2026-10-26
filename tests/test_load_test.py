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
        self.assertIn("cmd:load(playerid, params[])", COMMANDS)
        self.assertEqual(COMMANDS.count("LoadTest_Command(playerid, params);"), 2)

    def test_safety_gate_permissions_budgets_and_cleanup(self):
        for expected in (
            "!Admin_IsFounder(playerid)",
            "if (!gAdminDuty[playerid])",
            "!gAdminDuty[gLoadOwner]",
            "#define LOADTEST_MAX_NPCS        (100)",
            "#define LOADTEST_BATCH_NPCS      (4)",
            "#define LOADTEST_MAX_SECONDS     (300)",
            "LOADTEST_MIN_TICKRATE",
            "LOADTEST_BAD_SAMPLES",
            "LoadTest_Stop(\"owner_disconnect\")",
            "LoadTest_Stop(\"owner_unavailable\")",
            "NPC_Destroy(gLoadNPCIds[i]);",
            "KillTimer(gLoadTimer);",
        ):
            self.assertIn(expected, LOADTEST)
        self.assertNotIn("mysql_query", LOADTEST)
        self.assertNotIn("mysql_tquery", LOADTEST)
        self.assertIn("not a real-player", DOC.lower().replace("**", ""))
        self.assertIn("load_test_01_26.10.2026.csv", DOC)
        self.assertNotIn("LOADTEST_ENABLE_FLAG", LOADTEST)
        self.assertIn("LoadTest_CreateReport()", LOADTEST)
        self.assertIn("getdate(year, month, day)", LOADTEST)
        self.assertIn("if (fexist(gLoadReportFile)) continue;", LOADTEST)
        self.assertIn("fwrite(reportHandle, LOADTEST_CSV_HEADER)", LOADTEST)
        self.assertIn('if (!LoadTest_CreateReport())', LOADTEST)
        self.assertLess(LOADTEST.index('if (!LoadTest_CreateReport())'),
                        LOADTEST.index('gLoadRunning = true;'))
        self.assertIn('if (!strcmp(params, "start", true))', LOADTEST)
        self.assertIn('LOADTEST_DEFAULT_NPCS, LOADTEST_DEFAULT_SECONDS', LOADTEST)

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

    def test_new_header_and_legacy_without_header(self):
        header = ",".join(report.COLUMNS)
        row = "100,0,0,8,90,90,0,1000,0,start"
        with_header = self._read([header, row])
        without_header = self._read([row])
        self.assertEqual(with_header, without_header)
        self.assertEqual(len(with_header), 1)

    def test_header_only_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "no measurements"):
            self._read([",".join(report.COLUMNS)])

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
