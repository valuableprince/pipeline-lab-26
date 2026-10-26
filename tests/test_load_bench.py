"""LOADTEST-04: isolated synthetic account benchmark static and report contracts."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
NPC = (ROOT / "gamemodes/includes/load_test.inc").read_text(encoding="utf-8")
BENCH = (ROOT / "gamemodes/includes/load_bench.inc").read_text(encoding="utf-8")
MIGRATION = (ROOT / "db/migrations/013_loadtest_virtual_accounts.sql").read_text(encoding="utf-8")
TELEMETRY = (ROOT / "gamemodes/includes/load_telemetry.inc").read_text(encoding="utf-8")


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

    def test_exact_latency_telemetry_connected_to_real_operations(self):
        self.assertIn('#include "load_telemetry.inc"', GAME)
        self.assertIn('Telemetry_RecordSQL(ms);', BENCH)
        self.assertIn('Telemetry_RecordBcrypt(ms, verify);', BENCH)
        self.assertIn('Telemetry_StartCycle(slot);', BENCH)
        self.assertIn('Telemetry_FinishCycle(slot);', BENCH)
        self.assertIn('Telemetry_CountSecond(true);', BENCH)
        self.assertIn('Telemetry_CountSecond(false);', BENCH)
        self.assertIn('Telemetry_Sample(phase)', BENCH)
        self.assertIn('Telemetry_WriteSummary()', BENCH)
        self.assertIn('Telemetry_Progress(playerid);', BENCH)
        self.assertIn('Telemetry_Open()', BENCH)

    def test_quantiles_are_exact_bounded_nearest_rank(self):
        self.assertIn('#define TELEMETRY_MAX_SQL (LOADTEST_MAX_NPCS * 3)', TELEMETRY)
        self.assertIn('stock Telemetry_Quantile(const values[], count, percentile)', TELEMETRY)
        self.assertIn('sorted[(count * percentile + 99) / 100 - 1]', TELEMETRY)
        self.assertIn('new sorted[TELEMETRY_MAX_SQL]', TELEMETRY)
        self.assertIn('Telemetry_Quantile(gTelemetrySQL, gTelemetrySQLCount, 99)', TELEMETRY)
        self.assertIn('Telemetry_Quantile(gTelemetryHash, gTelemetryHashCount, 99)', TELEMETRY)
        self.assertIn('Telemetry_Quantile(gTelemetryVerify, gTelemetryVerifyCount, 99)', TELEMETRY)
        self.assertIn('Telemetry_Quantile(gTelemetryCycle, gTelemetryCycleCount, 99)', TELEMETRY)
        self.assertIn('if (count < 1 || count > TELEMETRY_MAX_SQL', TELEMETRY)

    def test_advanced_report_quality_and_unavailable_host_metrics(self):
        self.assertIn('_advanced.csv', TELEMETRY)
        self.assertIn('TELEMETRY_HEADER', TELEMETRY)
        self.assertIn('registration_rate_x100', TELEMETRY)
        self.assertIn('authentication_rate_x100', TELEMETRY)
        self.assertIn('peak_registrations_1s', TELEMETRY)
        self.assertIn('gTelemetryCycleCount', TELEMETRY)
        self.assertIn('gTelemetryRegPerSecond', TELEMETRY)
        self.assertIn('gTelemetryAuthPerSecond', TELEMETRY)
        self.assertIn('CPU_PERCENT=UNAVAILABLE', TELEMETRY)
        self.assertIn('RAM_MB=UNAVAILABLE', TELEMETRY)
        self.assertNotIn('UPDATE accounts ', TELEMETRY)

if __name__ == "__main__":
    unittest.main()
