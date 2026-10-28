"""GOV-04 structural contract tests; money never bypasses official escrow."""
from pathlib import Path
import unittest
from build_contracts import assert_staged

ROOT=Path(__file__).resolve().parents[1]
SQL=(ROOT/"db/migrations/022_bond_debt_crisis.sql").read_text("utf-8")
PAWN=(ROOT/"gamemodes/includes/gov_debt.inc").read_text("utf-8")
MAIN=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
DB=(ROOT/"gamemodes/includes/db_primary.inc").read_text("utf-8")
CI=(ROOT/".github/workflows/build.yml").read_text("utf-8")


class OverdueDebtContracts(unittest.TestCase):
    def test_additive_migration_and_financial_tables(self):
        for needle in (
            "ADD COLUMN bond_crisis_state","ADD COLUMN recovery_round",
            "ADD COLUMN restructure_revision","ADD COLUMN restructure_pending_plan",
            "ADD COLUMN debt_target_at","CREATE TABLE government_bond_recoveries",
            "CREATE TABLE government_bond_restructures",
            "CREATE TABLE government_bond_restructure_audit",
            "CREATE TABLE bond_crisis_events",
            "UNIQUE KEY uq_bond_recovery_event","UNIQUE KEY uq_bond_recovery_round",
            "INSERT INTO schema_migrations(version) VALUES (22);"):
            with self.subTest(needle=needle):self.assertIn(needle,SQL)
        self.assertNotIn("UPDATE accounts SET balance",SQL)
        self.assertNotIn("INSERT INTO treasury_ledger",SQL)
        self.assertNotIn("DELETE FROM government_bond_claims",SQL)
        self.assertNotIn("UPDATE government_bond_issues SET face_value",SQL)

    def test_recovery_transacts_accurate_escrow(self):
        x=SQL.split("CREATE PROCEDURE gov_bond_recovery_allocate(",1)[1].split("END$$",1)[0]
        for needle in (
            "START TRANSACTION;","ROLLBACK;","COMMIT;",
            "FROM treasury_accounts WHERE code='state' FOR UPDATE",
            "FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE",
            "FROM government_bond_recoveries WHERE event_key=p_event_key",
            "v_debt NOT IN ('DEFAULTED','RESTRUCTURED')",
            "v_insurance_key IS NULL","v_balance-v_min-v_fund-v_pool",
            "LEAST(v_free,p_budget_cap,v_max_tx,",
            "principal_due-principal_budget_awarded-insurance_awarded",
            "interest_due-interest_budget_awarded",
            "DECLARE cur CURSOR FOR","OPEN cur;","FETCH cur INTO",
            "CAST(v_principal_seen+v_claim_principal AS DECIMAL(30,0))",
            "CAST(v_interest_seen+v_claim_interest AS DECIMAL(30,0))",
            "v_principal_awarded<>v_principal_budget",
            "v_interest_awarded<>v_interest_budget",
            "principal_budget_awarded=principal_budget_awarded+v_principal_step",
            "interest_budget_awarded=interest_budget_awarded+v_interest_step",
            "bond_payout_reserved=bond_payout_reserved+v_budget",
            "VALUES(p_event_key,'RECOVERY'",
            "INSERT INTO government_bond_recoveries",
            "recovery_round=recovery_round+1",
            "debt_status=IF(v_principal_total+v_interest_total=v_budget,"):
            with self.subTest(needle=needle):self.assertIn(needle,x)
        self.assertNotIn("UPDATE accounts",x)

    def test_restructure_requires_distinct_approval_and_no_haircut(self):
        x=SQL.split("CREATE PROCEDURE gov_bond_restructure_propose(",1)[1]
        y=SQL.split("CREATE PROCEDURE gov_bond_restructure_approve(",1)[1]
        for needle in ("v_insurance_key IS NULL","v_pending<>0",
                       "v_outstanding=0","INSERT INTO government_bond_restructures",
                       "restructure_pending_plan=v_plan",
                       "INSERT INTO government_bond_restructure_audit"):
            self.assertIn(needle,x)
        for needle in ("v_proposer=p_actor","v_plan_revision<>v_revision+1",
                       "v_target<=UTC_TIMESTAMP()",
                       "v_remaining=0","status='SUPERSEDED'",
                       "status='APPROVED'","debt_target_at=v_target",
                       "debt_status='RESTRUCTURED'"):
            self.assertIn(needle,y)
        self.assertNotIn("UPDATE government_bond_claims SET",y)
        self.assertNotIn("maturity_at=v_target",y)
        self.assertNotIn("coverage_basis_points=",y)

    def test_automatic_poll_and_db_guard(self):
        for needle in ("stock GovDebt_Init()","stock GovDebt_Shutdown()",
            "stock bool:GovDebt_IsReady()","stock GovDebt_Reset(playerid)",
            "stock bool:GovDebt_Propose(","stock bool:GovDebt_Approve(",
            "Admin_Allowed(playerid,PERM_MANAGE_STAFF)",
            "public GovDebt_Tick()","public GovDebt_OnScan()",
            "public GovDebt_OnRecovered(issueId)","public GovDebt_OnCrisis()",
            "CALL gov_bond_recovery_allocate","CALL gov_bond_crisis_reconcile",
            "mysql_tquery(gDBPrimary","insurance_event_key IS NOT NULL",
            "ORDER BY i.maturity_at,i.issue_id LIMIT 1",
            "GovDebt_OnError(errorid,const callback[])"):
            with self.subTest(needle=needle):self.assertIn(needle,PAWN)
        self.assertIn('#include "gov_debt.inc"',MAIN)
        self.assertIn("    GovDebt_Init();",MAIN)
        self.assertIn("    GovDebt_Shutdown();",MAIN)
        self.assertEqual(MAIN.count("    GovDebt_Reset(playerid);"),2)
        self.assertIn("GovDebt_OnError(errorid, callback)",DB)
        self.assertIn("022_bond_debt_crisis.sql",CI)
        self.assertIn("gov04_mysql_uat.py",CI)
        assert_staged(self,"gov_debt.inc")


if __name__=="__main__":
    unittest.main()
