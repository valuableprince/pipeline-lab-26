"""GOV-02 contract tests: issue registry, locked terms and atomic approval."""
import unittest
from pathlib import Path
from build_contracts import assert_staged

ROOT=Path(__file__).resolve().parents[1]
SQL=(ROOT/"db/migrations/020_government_bond_issues.sql").read_text("utf-8")
PAWN=(ROOT/"gamemodes/includes/government_bonds.inc").read_text("utf-8")
MAIN=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
CI=(ROOT/".github/workflows/build.yml").read_text("utf-8")

class GovernmentBondIssueContracts(unittest.TestCase):
    def test_tables_and_no_money_creation(self):
        for item in ("CREATE TABLE government_bond_issues",
                     "CREATE TABLE government_bond_holdings",
                     "CREATE TABLE government_bond_issue_audit",
                     "PRIMARY KEY (issue_id, account_id)",
                     "UNIQUE KEY uq_government_bond_code",
                     "UNIQUE KEY uq_government_bond_create",
                     "UNIQUE KEY uq_bond_issue_event",
                     "insured_max_exposure",
                     "coverage_basis_points",
                     "premium_basis_points",
                     "created_by_account_id",
                     "approved_by_account_id",
                     "INSERT INTO schema_migrations(version) VALUES (20);"):
            with self.subTest(item=item): self.assertIn(item,SQL)
        self.assertNotIn("UPDATE accounts SET balance",SQL)
        self.assertNotIn("INSERT INTO treasury_ledger",SQL)
        self.assertNotIn("INSERT INTO government_bond_holdings",SQL)

    def test_approval_reserves_guarantee_and_issue_together(self):
        for item in ("CREATE PROCEDURE gov_bond_issue_create(",
                     "CREATE PROCEDURE gov_bond_issue_approve(",
                     "CREATE PROCEDURE gov_bond_issue_open(",
                     "CREATE PROCEDURE gov_bond_issue_cancel(",
                     "CREATE TRIGGER government_bond_terms_immutable",
                     "approved bond terms immutable",
                     "v_creator=p_actor_id",
                     "FROM treasury_accounts WHERE code='state' FOR UPDATE",
                     "FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE",
                     "INSERT INTO bond_fund_guarantees",
                     "INSERT INTO bond_fund_ledger",
                     "INSERT INTO government_bond_issue_audit",
                     "bond_fund_committed=bond_fund_committed+v_exposure",
                     "(CAST(v_face AS UNSIGNED) * v_coverage + 9999) DIV 10000",
                     "(v_committed + v_exposure) * 10000 > v_reserved * v_utilization",
                     "insured_max_exposure=v_exposure",
                     "coverage_basis_points=v_coverage",
                     "premium_basis_points=v_premium",
                     "status='APPROVED'",
                     "status='OFFERING'",
                     "v_sold <> 0",
                     "bond_fund_committed=bond_fund_committed-v_release",
                     "released_exposure=max_exposure",
                     "SET MESSAGE_TEXT='standalone commitments disabled: use gov_bond_issue_approve'"):
            with self.subTest(item=item): self.assertIn(item,SQL)
        for name in ("gov_bond_issue_create","gov_bond_issue_approve",
                     "gov_bond_issue_open","gov_bond_issue_cancel"):
            routine=SQL.split("CREATE PROCEDURE "+name+"(",1)[1].split("END$$",1)[0]
            self.assertIn("START TRANSACTION;",routine)
            self.assertIn("ROLLBACK;",routine)
            self.assertIn("COMMIT;",routine)
            self.assertIn("SQL SECURITY INVOKER",routine)

    def test_staged_pawn_adapter_and_ci(self):
        for token in ("stock BondIssues_Init()",
                      "stock BondIssues_Shutdown()",
                      "stock bool:BondIssues_CreateDraft(",
                      "stock bool:BondIssues_Approve(",
                      "stock bool:BondIssues_Open(",
                      "stock bool:BondIssues_Cancel(",
                      "Admin_Allowed(playerid, PERM_MANAGE_STAFF)",
                      "mysql_tquery(gDBPrimary, query, \"BondIssues_OnComplete\"",
                      "gBondIssuesPendingAccount[playerid]",
                      "gAuthToken[playerid] != authToken",
                      "BondFund_Reload()",
                      "WHERE version=20 LIMIT 1"):
            with self.subTest(token=token): self.assertIn(token,PAWN)
        self.assertIn('#include "government_bonds.inc"',MAIN)
        self.assertIn("    BondIssues_Init();",MAIN)
        self.assertIn("    BondIssues_Shutdown();",MAIN)
        self.assertIn("    BondIssues_Reset(playerid);",MAIN)
        self.assertNotIn("GivePlayerMoney(",PAWN)
        assert_staged(self,"government_bonds.inc")
        self.assertIn("020_government_bond_issues.sql",CI)
        self.assertIn("gov02_mysql_uat.py",CI)

if __name__=="__main__":
    unittest.main()
