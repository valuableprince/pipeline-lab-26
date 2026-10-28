"""GOV-00: strict static contracts for async atomic player <-> treasury settlement."""
import unittest
from pathlib import Path
from build_contracts import assert_staged

ROOT = Path(__file__).resolve().parents[1]
SQL = (ROOT / "db/migrations/018_gov_atomic_transfers.sql").read_text("utf-8")
PAWN = (ROOT / "gamemodes/includes/gov_transfer.inc").read_text("utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
DB = (ROOT / "gamemodes/includes/db_primary.inc").read_text("utf-8")
ECON = (ROOT / "gamemodes/includes/economy.inc").read_text("utf-8")
OLD = (ROOT / "db/migrations/017_treasury_core.sql").read_text("utf-8")


class Gov00Contracts(unittest.TestCase):
    def test_one_new_migration_no_rewrite_of_017(self):
        self.assertIn("CREATE TABLE gov_transfers", SQL)
        self.assertIn("INSERT INTO schema_migrations(version) VALUES (18)", SQL)
        self.assertIn("UNIQUE KEY uq_gov_transfer_event (event_key)", SQL)
        self.assertIn("FOREIGN KEY (account_id)", SQL)
        self.assertNotIn("ALTER TABLE treasury_accounts", SQL)
        self.assertNotIn("ALTER TABLE accounts", SQL)
        self.assertNotIn("DROP TABLE", SQL)
        self.assertIn("CREATE PROCEDURE treasury_post", OLD)

    def test_transaction_atomically_updates_both_wallets_and_logs(self):
        for fragment in (
            "CREATE PROCEDURE gov_transfer_post(",
            "SQL SECURITY INVOKER",
            "START TRANSACTION;",
            "FROM treasury_accounts WHERE code='state' FOR UPDATE;",
            "FROM accounts WHERE account_id=p_account_id FOR UPDATE;",
            "FROM gov_transfers WHERE event_key=p_event_key LIMIT 1;",
            "v_existing_account <> p_account_id",
            "v_existing_delta <> p_treasury_delta",
            "BINARY v_existing_source <> BINARY p_source",
            "p_expected_account_version",
            "v_account_version <> p_expected_account_version",
            "v_account_serial >= 1000000000",
            "v_treasury_version >= 2147483646",
            "v_account < p_treasury_delta",
            "v_treasury - v_minimum_reserve < -p_treasury_delta",
            "UPDATE treasury_accounts",
            "UPDATE accounts",
            "INSERT INTO treasury_ledger",
            "INSERT INTO gov_transfers",
            "COMMIT;",
            "ROLLBACK;",
            "RESIGNAL;",
            "v_replayed AS replayed",
        ):
            with self.subTest(fragment=fragment):
                self.assertIn(fragment, SQL)
        self.assertLess(SQL.index("START TRANSACTION;"), SQL.index("FROM accounts WHERE"))
        self.assertLess(SQL.index("UPDATE accounts"), SQL.index("INSERT INTO treasury_ledger"))
        self.assertLess(SQL.index("INSERT INTO gov_transfers"), SQL.index("COMMIT;"))
        self.assertNotIn("DROP PROCEDURE treasury_post", SQL)

    def test_async_pawn_api_and_fail_closed_snapshot(self):
        for fragment in (
            "stock GovTransfer_Init()",
            "stock GovTransfer_Shutdown()",
            "stock bool:GovTransfer_IsReady()",
            "stock bool:GovTransfer_Request(",
            "public GovTransfer_OnComplete(",
            "mysql_tquery(gDBPrimary, query, \"GovTransfer_OnComplete\"",
            "gGovTransferPendingCount >= GOV_TRANSFER_MAX_PENDING",
            "gGovTransferBusy[playerid] = true;",
            "gAuthToken[playerid] != authToken",
            "gDBPrimaryAccountId[playerid] != accountId",
            "gDBPrimaryVersion[playerid] != gGovTransferExpectedVersion[playerid]",
            "gGovTransferReady = false;",
            "Kick(playerid);",
            "gAccountMoney[playerid] = balance;",
            "gAccountTxnSerial[playerid] = serial;",
            "gDBPrimaryVersion[playerid] = accountVersion;",
            "gTreasuryBalance = treasury;",
        ):
            with self.subTest(fragment=fragment):
                self.assertIn(fragment, PAWN)
        self.assertNotIn("mysql_query(gDBPrimary, query);", PAWN)
        self.assertNotIn("Economy_Credit(", PAWN)
        self.assertNotIn("Economy_Debit(", PAWN)
        assert_staged(self, "gov_transfer.inc")

    def test_lifecycle_lock_and_safe_deactivation(self):
        self.assertIn("new bool:gGovTransferBusy[MAX_PLAYERS]", MAIN)
        self.assertIn('#include "gov_transfer.inc"', MAIN)
        self.assertLess(MAIN.index('#include "treasury.inc"'), MAIN.index('#include "gov_transfer.inc"'))
        self.assertLess(MAIN.index("    Treasury_Init();"), MAIN.index("    GovTransfer_Init();"))
        self.assertLess(MAIN.index("    GovTransfer_Shutdown();"), MAIN.index("    Treasury_Shutdown();"))
        self.assertLess(MAIN.index("    DBPrimary_OnConnect(playerid);"), MAIN.index("    GovTransfer_OnConnect(playerid);"))
        self.assertLess(MAIN.index("    GovTransfer_OnDisconnect(playerid);"), MAIN.index("    DBPrimary_OnDisconnect(playerid);"))
        self.assertIn("if (gGovTransferBusy[playerid]) return false;", DB)
        self.assertIn("if (gGovTransferBusy[playerid]) return false;", ECON)
        self.assertIn("WHERE version=18 LIMIT 1", PAWN)
        self.assertIn("#if defined TARKENSO_DB_PRIMARY", PAWN)


if __name__ == "__main__":
    unittest.main()
