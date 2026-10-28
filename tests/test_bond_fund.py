"""GOV-01: protected bond insurance reserve structural safety contracts."""
import unittest
from pathlib import Path
from build_contracts import assert_staged

ROOT=Path(__file__).resolve().parents[1]
SQL=(ROOT/"db/migrations/019_bond_fund_core.sql").read_text("utf-8")
PAWN=(ROOT/"gamemodes/includes/bond_fund.inc").read_text("utf-8")
MAIN=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
CICD=(ROOT/".github/workflows/build.yml").read_text("utf-8")
V17=(ROOT/"db/migrations/017_treasury_core.sql").read_text("utf-8")
V18=(ROOT/"db/migrations/018_gov_atomic_transfers.sql").read_text("utf-8")

class BondFundContracts(unittest.TestCase):
    def test_additive_schema_and_defaults(self):
        for value in (
            "ADD COLUMN bond_fund_reserved BIGINT UNSIGNED NOT NULL DEFAULT 0",
            "ADD COLUMN bond_fund_committed BIGINT UNSIGNED NOT NULL DEFAULT 0",
            "ADD COLUMN bond_fund_enabled TINYINT UNSIGNED NOT NULL DEFAULT 0",
            "ADD COLUMN bond_fund_coverage_bps SMALLINT UNSIGNED NOT NULL DEFAULT 2000",
            "ADD COLUMN bond_fund_premium_bps SMALLINT UNSIGNED NOT NULL DEFAULT 200",
            "ADD COLUMN bond_fund_utilization_bps SMALLINT UNSIGNED NOT NULL DEFAULT 8000",
            "ADD COLUMN bond_fund_tax_allocation_bps SMALLINT UNSIGNED NOT NULL DEFAULT 500",
            "ADD COLUMN bond_fund_grace_seconds INT UNSIGNED NOT NULL DEFAULT 86400",
            "CONSTRAINT chk_bond_fund_balance CHECK (bond_fund_reserved <= balance)",
            "bond_fund_committed * 10000 <= bond_fund_reserved * bond_fund_utilization_bps",
            "CREATE TABLE bond_fund_ledger",
            "CREATE TABLE bond_fund_guarantees",
            "UNIQUE KEY uq_bond_fund_event (event_key)",
            "UNIQUE KEY uq_bond_guarantee_event (event_key)",
            "INSERT INTO schema_migrations(version) VALUES (19);",
        ):
            with self.subTest(value=value): self.assertIn(value,SQL)
        self.assertNotIn("INSERT INTO treasury_accounts",SQL)
        self.assertNotIn("DROP TABLE",SQL)
        self.assertNotIn("ALTER TABLE accounts",SQL)
        self.assertIn("CREATE PROCEDURE treasury_post(",V17)
        self.assertIn("CREATE PROCEDURE gov_transfer_post(",V18)

    def test_replacement_debit_routines_preserve_fund(self):
        for value in (
            "DROP PROCEDURE IF EXISTS treasury_post$$",
            "DROP PROCEDURE IF EXISTS gov_transfer_post$$",
            "CREATE PROCEDURE treasury_post(",
            "CREATE PROCEDURE gov_transfer_post(",
            "SQL SECURITY INVOKER",
            "bond_fund_reserved",
            "IF v_balance < v_reserve + v_fund_reserved THEN",
            "IF v_balance - v_reserve - v_fund_reserved < -p_delta THEN",
            "OR v_treasury < v_minimum_reserve + v_fund_reserved",
            "IF v_treasury - v_minimum_reserve - v_fund_reserved < -p_treasury_delta THEN",
            "START TRANSACTION;",
            "ROLLBACK;",
            "COMMIT;",
            "INSERT INTO treasury_ledger",
            "INSERT INTO gov_transfers",
        ):
            with self.subTest(value=value): self.assertIn(value,SQL)

    def test_fund_operations_are_atomic_and_idempotent(self):
        for value in (
            "CREATE PROCEDURE bond_fund_capitalize(",
            "CREATE PROCEDURE bond_fund_commit(",
            "WHERE code='state' FOR UPDATE;",
            "FROM bond_fund_ledger WHERE event_key=p_event_key LIMIT 1;",
            "fund event collision",
            "bond_fund_reserved=bond_fund_reserved+p_amount",
            "bond_fund_committed=bond_fund_committed+p_exposure",
            "(v_committed + p_exposure) * 10000 > v_reserved * v_utilization",
            "INSERT INTO bond_fund_guarantees",
            "INSERT INTO bond_fund_ledger",
            "v_replayed AS replayed",
        ):
            with self.subTest(value=value): self.assertIn(value,SQL)
        self.assertNotIn("CREATE PROCEDURE bond_fund_insurance_payout",SQL)
        self.assertNotIn("UPDATE accounts SET balance",SQL)

    def test_pawn_integration_no_money_generation(self):
        for value in (
            "stock BondFund_Init()",
            "stock BondFund_Shutdown()",
            "stock bool:BondFund_Reload()",
            "stock bool:BondFund_IsReady()",
            "stock BondFund_GetReserved()",
            "stock BondFund_GetCommitted()",
            "stock BondFund_GetAvailableCapacity()",
            "stock BondFund_GetFreeBudget()",
            "stock bool:BondFund_Capitalize(",
            "stock bool:BondFund_Commit(",
            "mysql_tquery(gDBPrimary, query, \"BondFund_OnComplete\"",
            "gBondFundBusy = true;",
            "gBondFundReady = false;",
            "gTreasuryReady = false;",
            "WHERE version=19 LIMIT 1",
        ):
            with self.subTest(value=value): self.assertIn(value,PAWN)
        self.assertIn('#include "bond_fund.inc"',MAIN)
        self.assertLess(MAIN.index('include "gov_transfer.inc"'),MAIN.index('include "bond_fund.inc"'))
        self.assertLess(MAIN.index('    GovTransfer_Init();'),MAIN.index('    BondFund_Init();'))
        self.assertLess(MAIN.index('    BondFund_Shutdown();'),MAIN.index('    GovTransfer_Shutdown();'))
        self.assertNotIn("GivePlayerMoney(",PAWN)
        self.assertNotIn("Economy_Credit(",PAWN)
        assert_staged(self,"bond_fund.inc")
        self.assertIn("019_bond_fund_core.sql",CICD)
        self.assertIn("gov01_mysql_uat.py",CICD)

if __name__ == "__main__":
    unittest.main()
