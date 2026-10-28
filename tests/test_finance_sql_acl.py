"""Unit tests for the read-only GOV-06 runtime SQL ACL gate."""
import unittest

from tools.check_finance_sql_acl import assess_grants


class FinanceAclTests(unittest.TestCase):
    def check(self, grants, status):
        result = assess_grants(grants, "tarkenso_gov00")
        self.assertEqual(result["status"], status, result)
        return result

    def test_limited_auditor_can_pass(self):
        result = self.check([
            "GRANT USAGE ON *.* TO 'auditor'@'%'",
            "GRANT SELECT ON \x60tarkenso_gov00\x60.\x60treasury_accounts\x60 TO 'auditor'@'%'",
            "GRANT SELECT ON \x60tarkenso_gov00\x60.\x60government_bond_issues\x60 TO 'auditor'@'%'",
            "GRANT EXECUTE ON PROCEDURE \x60tarkenso_gov00\x60.\x60gov_finance_assert_integrity\x60 TO 'auditor'@'%'",
        ], "PASS_FINANCE_ACL_ONLY")
        self.assertTrue(any("separate review" in w for w in result["warnings"]))

    def test_full_schema_grant_is_rejected(self):
        self.check(["GRANT ALL PRIVILEGES ON \x60tarkenso_gov00\x60.* TO 'app'@'%'"],
                   "FAIL")

    def test_global_dml_and_execute_are_rejected(self):
        self.check(["GRANT SELECT, INSERT, UPDATE, EXECUTE ON *.* TO 'app'@'%'"],
                   "FAIL")

    def test_finance_table_write_is_rejected(self):
        result = self.check([
            "GRANT SELECT, UPDATE ON \x60tarkenso_gov00\x60.\x60treasury_accounts\x60 TO 'app'@'%'"
        ], "FAIL")
        self.assertTrue(any("treasury_accounts" in v for v in result["violations"]))

    def test_accounts_balance_write_is_rejected(self):
        self.check([
            "GRANT UPDATE ON \x60tarkenso_gov00\x60.\x60accounts\x60 TO 'app'@'%'"
        ], "FAIL")
        self.check([
            "GRANT UPDATE (balance, health) ON \x60tarkenso_gov00\x60.\x60accounts\x60 TO 'app'@'%'"
        ], "FAIL")

    def test_column_limited_cosmetic_update_is_only_warning(self):
        result = self.check([
            "GRANT UPDATE (health, armour) ON \x60tarkenso_gov00\x60.\x60accounts\x60 TO 'app'@'%'"
        ], "PASS_FINANCE_ACL_ONLY")
        self.assertTrue(any("column-limited" in w for w in result["warnings"]))

    def test_account_insert_is_rejected(self):
        self.check([
            "GRANT INSERT ON \x60tarkenso_gov00\x60.\x60accounts\x60 TO 'app'@'%'"
        ], "FAIL")

    def test_role_and_unparseable_statements_fail_closed(self):
        self.check(["GRANT 'operator'@'%' TO 'app'@'%'"], "FAIL")
        self.check(["some unknown grant syntax"], "FAIL")

    def test_grant_option_is_rejected(self):
        self.check([
            "GRANT SELECT ON \x60tarkenso_gov00\x60.\x60treasury_accounts\x60 TO 'app'@'%' WITH GRANT OPTION"
        ], "FAIL")

    def test_financial_proc_can_be_called_but_not_certified_safe(self):
        result = self.check([
            "GRANT EXECUTE ON PROCEDURE \x60tarkenso_gov00\x60.\x60treasury_post\x60 TO 'app'@'%'"
        ], "PASS_FINANCE_ACL_ONLY")
        self.assertTrue(any("DEFINER" in w for w in result["warnings"]))

    def test_gameplay_dml_outside_finance_is_not_confused_with_cash_grant(self):
        self.check([
            "GRANT SELECT, INSERT, UPDATE, DELETE ON \x60tarkenso_gov00\x60.\x60cleaner_profiles\x60 TO 'app'@'%'"
        ], "PASS_FINANCE_ACL_ONLY")

    def test_empty_grants_fails(self):
        self.check([], "FAIL")


if __name__ == "__main__":
    unittest.main()
