"""AUDIT-STAGE-01: regression contracts for PayDay and non-disruptive DB failure recovery."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
PAYDAY = (ROOT / "gamemodes/includes/payday.inc").read_text(encoding="utf-8")
DB = (ROOT / "gamemodes/includes/db_primary.inc").read_text(encoding="utf-8")


class PaydayReliabilityContracts(unittest.TestCase):
    def test_due_hours_survive_transient_sql_failure(self):
        for marker in (
            "gPayDayPendingHour[MAX_PLAYERS]",
            "gPayDayPendingThrough[MAX_PLAYERS]",
            "gPayDayNextRetry[MAX_PLAYERS]",
            "PayDay_Grant(playerid, pending)",
            "now + PAYDAY_RETRY_SECONDS",
            "PAYDAY_GRANTS_PER_TICK",
        ):
            with self.subTest(marker=marker):
                self.assertIn(marker, PAYDAY)
        self.assertIn("gPayDayPendingHour[playerid] = pending;", PAYDAY)

    def test_pending_is_bound_to_player_session(self):
        self.assertGreaterEqual(GAME.count("PayDay_Reset(playerid);"), 2)
        self.assertIn("stock PayDay_Reset(playerid)", PAYDAY)
        self.assertIn("for (new i = 0; i < MAX_PLAYERS; i++) PayDay_Reset(i);", PAYDAY)

    def test_payout_has_database_uniqueness_guard(self):
        self.assertIn("last_payday_hour<%d", PAYDAY)
        self.assertIn("row_version=row_version+1", PAYDAY)
        self.assertIn("gCharacterLastPaydayHour[playerid] >= pending", PAYDAY)


class DatabaseRecoveryContracts(unittest.TestCase):
    def test_error_does_not_mass_disconnect_authenticated_users(self):
        error_handler = DB.split("public OnQueryError(", 1)[1].split("\n#else", 1)[0]
        self.assertNotIn("Kick(i);", error_handler)
        self.assertNotIn("Kick(playerid);", error_handler)
        self.assertNotIn('printf("%s", query)', error_handler)

    def test_transport_errors_in_synchronous_writes_are_detected(self):
        execute = DB.split("stock bool:DBPrimary_Execute(", 1)[1].split(
            "stock bool:DBPrimary_AccountExists(", 1)[0]
        handler = DB.split("public OnQueryError(", 1)[1].split(
            "\\n#else", 1)[0]
        self.assertIn("new code = mysql_errno(gDBPrimary);", execute)
        self.assertIn("DBPrimary_MarkDegraded(code);", execute)
        self.assertIn("DBPrimary_MarkDegraded(errorid);", handler)
        transport = DB.split("stock bool:DBPrimary_MarkDegraded(", 1)[1].split(
            "stock bool:DBPrimary_Execute(", 1)[0]
        for code in ("2002", "2003", "2006", "2013", "2055"):
            self.assertIn(code, transport)
        self.assertIn("gDBPrimaryReady = false;", transport)
        self.assertNotIn("Kick(i)", transport)

    def test_connection_errors_pause_writes_and_probe_asynchronously(self):
        for marker in (
            "gDBPrimaryDegraded = true;",
            "gDBPrimaryReady = false;",
            "DBPrimary_StartRecoveryMonitor();",
            "public DBPrimary_RecoveryTick()",
            "public DBPrimary_ProbeDone()",
            '"SELECT version FROM schema_migrations WHERE version=10 LIMIT 1"',
            "mysql_tquery(gDBPrimary,",
            "gDBPrimaryProbeSent = gettime();",
        ):
            with self.subTest(marker=marker):
                self.assertIn(marker, DB)

    def test_account_lookup_timeout_has_session_token(self):
        self.assertIn("DBPrimary_AccountTimeout(playerid, token)", DB)
        self.assertIn("token != gAuthToken[playerid]", DB)
        self.assertIn("gDBPrimaryAuthPending[playerid]", DB)


if __name__ == "__main__":
    unittest.main()
