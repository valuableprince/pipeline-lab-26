"""LOADTEST-04: isolated synthetic account benchmark static and report contracts."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
NPC = (ROOT / "gamemodes/includes/load_test.inc").read_text(encoding="utf-8")
BENCH = (ROOT / "gamemodes/includes/load_bench.inc").read_text(encoding="utf-8")
MIGRATION = (ROOT / "db/migrations/013_loadtest_virtual_accounts.sql").read_text(encoding="utf-8")


class BenchContracts(unittest.TestCase):
    def test_source_wiring_and_scenarios(self):
        self.assertIn('#include "load_bench.inc"', GAME)
        self.assertLess(GAME.index('#include "db_primary.inc"'),
                        GAME.index('#include "load_bench.inc"'))
        self.assertIn("Bench_Begin(playerid)", NPC)
        self.assertIn("Bench_Tick();", NPC)
        self.assertIn("Bench_Finish(reason)", NPC)
        self.assertIn("Bench_Progress(playerid)", NPC)
        self.assertIn('"full"', NPC)
        self.assertIn('"npc"', NPC)
        self.assertIn("bool:fullScenario = false", NPC)
        self.assertIn("LOADTEST_BATCH_NPCS", NPC)

    def test_sql_is_isolated(self):
        self.assertIn("CREATE TABLE loadtest_virtual_accounts", MIGRATION)
        self.assertIn("PRIMARY KEY (run_key, virtual_id)", MIGRATION)
        self.assertIn("VALUES (13)", MIGRATION)
        for fragment in (
            "INSERT INTO loadtest_virtual_accounts",
            "SELECT password_hash FROM loadtest_virtual_accounts",
            "UPDATE loadtest_virtual_accounts",
            'cache_get_value_name(0, "password_hash"',
        ):
            self.assertIn(fragment, BENCH)
        for forbidden in ("UPDATE accounts ", "DELETE FROM accounts", "INSERT INTO accounts",
                          "UPDATE municipal_", "SET gAccountMoney", "SendRconCommand"):
            self.assertNotIn(forbidden, BENCH)

    def test_bcrypt_is_real_asynchronous_with_token_validation(self):
        self.assertIn("bcrypt_hash(gBenchOwner", BENCH)
        self.assertIn("bcrypt_verify(playerid", BENCH)
        self.assertIn("mysql_tquery(gDBPrimary", BENCH)
        self.assertIn("gBenchToken++", BENCH)
        self.assertIn("token == gBenchToken", BENCH)
        self.assertIn("if (!Bench_Valid(", BENCH)
        self.assertIn("BENCH_HASH_LIMIT (2)", BENCH)
        self.assertNotIn("Kick(playerid)", BENCH)
        self.assertNotIn("Auth_SaveAccount(playerid)", BENCH)

    def test_metrics_and_safety_verdict(self):
        for field in (
            "sql_completed", "sql_errors", "sql_avg_ms", "sql_max_ms",
            "hash_completed", "hash_avg_ms", "verify_completed", "verify_avg_ms",
            "tickrate", "heartbeat_ms", "heartbeat_delays_delta", "phase"
        ):
            self.assertIn(field, BENCH)
        for fragment in (
            "gBenchSQLBuckets", "gBenchBcryptBuckets", "Bench_P95Bucket(",
            "gBenchSaved == gLoadNPCTarget", "gBenchWorstRate >= LOADTEST_MIN_TICKRATE",
            '"PASS"', '"FAIL"', '"INCOMPLETE"', "metrics.csv", "summary.txt"
        ):
            self.assertIn(fragment, BENCH)
        self.assertIn("mysql_query(gDBPrimary,", BENCH)
        self.assertIn("version=13 LIMIT 1", BENCH)

if __name__ == "__main__":
    unittest.main()
