"""LOADTEST continuous activity and controlled NPC pacing regression contract."""
from pathlib import Path
import unittest
P = Path(__file__).resolve().parents[1]
NPC=(P/"gamemodes/includes/load_test.inc").read_text(encoding="utf-8")
BENCH=(P/"gamemodes/includes/load_bench.inc").read_text(encoding="utf-8")

class SustainedContracts(unittest.TestCase):
    def test_spawn_pacing(self):
        self.assertIn("gLoadSpawnRate = 1", NPC)
        self.assertIn("spawnRate < 1 || spawnRate > LOADTEST_BATCH_NPCS",NPC)
        self.assertIn("batch < gLoadSpawnRate",NPC)
        self.assertIn('sscanf(args, "iis[12]i", count, seconds, scenario, pace)', NPC)
        self.assertIn("LoadTest_Start(playerid, count, seconds, true, pace)",NPC)
    def test_bounded_sustained_workload(self):
        self.assertIn("gBenchPendingSQL < BENCH_SQL_LIMIT",BENCH)
        self.assertIn("dispatched < 4",BENCH)
        self.assertIn("gBenchStage[slot] != 6",BENCH)
        self.assertIn("gBenchNextAction[slot]",BENCH)
        self.assertIn("gBenchStage[slot] = 8",BENCH)
        self.assertIn("gBenchStage[slot] = 9",BENCH)
        self.assertIn('public Bench_OnActivityRead',BENCH)
        self.assertIn('public Bench_OnActivityWrite',BENCH)
        self.assertIn("Bench_Valid(playerid, token, slot, 8)",BENCH)
        self.assertIn("Bench_Valid(playerid, token, slot, 9)",BENCH)
    def test_isolation_and_reports(self):
        self.assertIn("SELECT balance FROM loadtest_virtual_accounts",BENCH)
        self.assertIn("UPDATE loadtest_virtual_accounts",BENCH)
        self.assertNotIn("UPDATE accounts ",BENCH)
        self.assertNotIn("PayDay_OnReady", BENCH)
        self.assertIn('activity.csv',BENCH)
        self.assertIn("payday_like_writes",BENCH)
        self.assertIn("gBenchActionErrors == 0",BENCH)

if __name__=="__main__":
    unittest.main()
