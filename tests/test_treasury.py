"""TREASURY-01: schema and Pawn API regression contracts (no live DB required)."""
import re
import unittest
from pathlib import Path
from build_contracts import assert_staged

ROOT = Path(__file__).resolve().parents[1]
SQL = (ROOT / "db/migrations/017_treasury_core.sql").read_text("utf-8")
PAWN = (ROOT / "gamemodes/includes/treasury.inc").read_text("utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
ECON = (ROOT / "gamemodes/includes/economy.inc").read_text("utf-8")


class TreasuryContract(unittest.TestCase):
    def test_schema_and_seed(self):
        for table in ("treasury_accounts", "treasury_ledger"):
            self.assertIn("CREATE TABLE " + table, SQL)
        for token in (
            "balance BIGINT UNSIGNED NOT NULL DEFAULT 0",
            "row_version INT UNSIGNED NOT NULL DEFAULT 0",
            "income_enabled TINYINT UNSIGNED NOT NULL DEFAULT 1",
            "expense_enabled TINYINT UNSIGNED NOT NULL DEFAULT 1",
            "minimum_reserve BIGINT UNSIGNED NOT NULL DEFAULT 0",
            "max_transaction BIGINT UNSIGNED NOT NULL DEFAULT 1000000000",
            "income_tax_basis_points SMALLINT UNSIGNED NOT NULL DEFAULT 0",
            "UNIQUE KEY uq_treasury_event (treasury_id,event_key)",
            "FOREIGN KEY (actor_account_id)",
            "INSERT INTO treasury_accounts(treasury_id,code,balance) VALUES (1,'state',0);",
            "INSERT INTO schema_migrations(version) VALUES (17);",
        ):
            self.assertIn(token, SQL)
        # State and policy live on the same locked account row.
        accounts_ddl = SQL.split("CREATE TABLE treasury_accounts (", 1)[1].split("CREATE TABLE treasury_ledger (", 1)[0]
        for column in ("balance", "income_enabled", "expense_enabled", "minimum_reserve",
                       "max_transaction", "income_tax_basis_points", "config_version"):
            self.assertIn(column, accounts_ddl)
        self.assertNotIn("CREATE TABLE treasury_settings", SQL)
        self.assertNotIn("JOIN treasury_settings", SQL)
        self.assertNotIn("ALTER TABLE accounts", SQL)

    def test_atomic_ledger_and_replay(self):
        for token in (
            "CREATE PROCEDURE treasury_post(",
            "SQL SECURITY INVOKER",
            "DECLARE EXIT HANDLER FOR SQLEXCEPTION",
            "ROLLBACK;",
            "RESIGNAL;",
            "START TRANSACTION;",
            "FOR UPDATE;",
            "WHERE treasury_id=p_treasury_id AND event_key=p_event_key LIMIT 1;",
            "v_existing_delta <> p_delta",
            "v_existing_actor <=> NULLIF(p_actor_id,0)",
            "treasury event key collision",
            "v_balance - v_reserve < -p_delta",
            "SET balance=v_new_balance, row_version=row_version+1",
            "INSERT INTO treasury_ledger(",
            "COMMIT;",
            "v_replayed AS replayed",
        ):
            self.assertIn(token, SQL)
        self.assertLess(SQL.index("START TRANSACTION;"), SQL.index("FOR UPDATE;"))
        self.assertLess(SQL.index("FOR UPDATE;"), SQL.index("INSERT INTO treasury_ledger("))
        self.assertLess(SQL.index("INSERT INTO treasury_ledger("), SQL.index("COMMIT;"))
        self.assertNotIn("DROP TABLE", SQL)

    def test_server_lifecycle_and_backend(self):
        for symbol in (
            "stock Treasury_Init()",
            "stock Treasury_Shutdown()",
            "stock bool:Treasury_Reload()",
            "stock bool:Treasury_IsReady()",
            "stock bool:Treasury_Post(",
            "stock bool:Treasury_Credit(",
            "stock bool:Treasury_Debit(",
            "stock Treasury_GetBalance()",
            "#if defined TARKENSO_DB_PRIMARY",
            "WHERE version=17 LIMIT 1",
            "mysql_format(gDBPrimary",
            "CALL treasury_post(",
            "gTreasuryLastReplayed = replayed != 0;",
            "gTreasuryReady = false;",
        ):
            self.assertIn(symbol, PAWN)
        self.assertIn("FROM treasury_accounts WHERE treasury_id=1 LIMIT 1", PAWN)
        self.assertNotIn("JOIN treasury_settings", PAWN)
        self.assertIn('#include "treasury.inc"', MAIN)
        self.assertLess(MAIN.index('#include "db_primary.inc"'),
                        MAIN.index('#include "treasury.inc"'))
        self.assertLess(MAIN.index("if (!DBPrimary_Init())"), MAIN.index("    Treasury_Init();"))
        self.assertLess(MAIN.index("    Treasury_Shutdown();"),
                        MAIN.index("    DBPrimary_Shutdown();"))
        assert_staged(self, "treasury.inc")

    def test_no_automatic_player_money_movement(self):
        self.assertNotIn("Economy_Credit(", PAWN)
        self.assertNotIn("Economy_Debit(", PAWN)
        self.assertNotIn("GivePlayerMoney(", PAWN)
        self.assertNotIn("treasury_post", ECON)
        self.assertIn("if (!Treasury_IsReady()", PAWN)
        self.assertIn("Treasury_ValidToken(eventKey, 64)", PAWN)
        self.assertIn("Treasury_ValidToken(source, 32)", PAWN)


if __name__ == "__main__":
    unittest.main()
