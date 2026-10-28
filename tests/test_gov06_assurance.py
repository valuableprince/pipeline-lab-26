"""GOV-06 static assurance contracts (separate from real MariaDB UAT)."""
import re
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SQL=(ROOT/"db/migrations/024_finance_assurance.sql").read_text("utf-8")
PAWN=(ROOT/"gamemodes/includes/bond_governance_ui.inc").read_text("utf-8")
CI=(ROOT/".github/workflows/build.yml").read_text("utf-8")


class Gov06AssuranceContracts(unittest.TestCase):
    def test_append_only_complete_journals(self):
        journals=(
            "treasury_ledger","bond_fund_ledger","bond_payout_ledger",
            "government_bond_issue_audit","government_bond_orders",
            "government_bond_claim_receipts","government_bond_recoveries",
            "government_bond_restructure_audit","government_bond_role_audit",
            "bond_crisis_events","government_bond_tax_earmarks",
        )
        triggers=re.findall(
            r"CREATE TRIGGER\s+(\w+)\s+BEFORE\s+(UPDATE|DELETE)\s+ON\s+(\w+)",
            SQL)
        self.assertEqual(len(triggers),len(journals)*2)
        self.assertEqual(
            {(event,table) for _,event,table in triggers},
            {(event,table) for table in journals for event in ("UPDATE","DELETE")})
        self.assertEqual(len(set(name for name,_,_ in triggers)),len(triggers))
        self.assertEqual(SQL.count("SIGNAL SQLSTATE '45000'"),len(triggers)+4)
        self.assertIn("INSERT INTO schema_migrations(version) VALUES (24);",SQL)

    def test_legacy_routines_are_definer_and_audit_is_read_only(self):
        for routine in ("treasury_post","gov_transfer_post","bond_fund_capitalize",
            "gov_bond_buy","gov_bond_claim_collect","gov_bond_recovery_allocate",
            "gov_bond_issue_create_auto",
            "gov_bond_tax_earmark","gov_bond_official_approve"):
            self.assertIn(f"ALTER PROCEDURE {routine} SQL SECURITY DEFINER;",SQL)
        block=SQL.split("CREATE PROCEDURE gov_finance_assert_integrity()",1)[1]
        self.assertIn("SQL SECURITY DEFINER",block)
        self.assertIn("v_balance<>v_treasury_ledger",block)
        self.assertIn("v_fund<>v_fund_ledger",block)
        self.assertIn("v_committed<>v_commit_ledger",block)
        self.assertIn("v_payout<>v_payout_ledger",block)
        self.assertIn("v_payout<>v_claim_escrow",block)
        self.assertIn("v_quantity_mismatches<>0",block)
        self.assertIn("v_owner_mismatches<>0",block)
        self.assertIn("v_receipt_mismatches<>0",block)
        self.assertIn("v_order_mismatches<>0",block)
        self.assertNotIn("UPDATE treasury_accounts",block)
        self.assertNotIn("DELETE FROM government_bond",block)

    def test_ministry_uses_one_connection_and_real_faction_membership(self):
        self.assertNotIn("mysql-govadmin.ini",PAWN)
        self.assertNotIn("gov_bond_role_set",PAWN)
        self.assertNotIn("government_bond_officials",PAWN)
        self.assertIn("faction_members",PAWN)
        self.assertIn("finance_rank",PAWN)
        self.assertIn("GovBond_UI_OnTaxPost",PAWN)
        self.assertIn("gov_bond_official_approve",PAWN)

    def test_ci_runs_after_all_previous_stages(self):
        markers=[
            "tests/gov04_mysql_uat.py","tests/gov05_mysql_uat.py",
            "db/migrations/024_finance_assurance.sql","tests/gov06_mysql_uat.py",
        ]
        for m in markers:self.assertIn(m,CI)
        self.assertEqual(sorted(CI.index(m) for m in markers),
                         [CI.index(m) for m in markers])


if __name__=="__main__":
    unittest.main()
