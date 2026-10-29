"""GOV-09 static contracts; MariaDB financial integration runs in the SQL CI job."""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
SQL=(ROOT/"db/migrations/030_gov09_bond_buyback.sql").read_text("utf-8")
UI=(ROOT/"gamemodes/includes/bonds_ui.inc").read_text("utf-8")
BACKEND=(ROOT/"gamemodes/includes/bond_settlement.inc").read_text("utf-8")
GOV=(ROOT/"gamemodes/includes/bond_governance_ui.inc").read_text("utf-8")
CI=(ROOT/".github/workflows/build.yml").read_text("utf-8")

class Gov09Contracts(unittest.TestCase):
    def test_price_and_finance_guards(self):
        self.assertIn("SET v_unit=CAST(v_face AS UNSIGNED)*98 DIV 100",SQL)
        for item in ("v_balance<v_min+v_fund+v_pool+v_payment",
                     "v_payment>v_max_tx",
                     "v_account>1000000000-v_payment",
                     "v_account_version<>p_expected_version",
                     "v_maturity_key IS NOT NULL",
                     "UTC_TIMESTAMP()>=v_maturity",
                     "SELECT balance,row_version,txn_serial",
                     "FROM accounts WHERE account_id=p_account_id FOR UPDATE",
                     "FROM government_bond_issues WHERE issue_id=p_issue_id FOR UPDATE",
                     "START TRANSACTION;",
                     "DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;",
                     "COMMIT;",
                     "INSERT INTO treasury_ledger",
                     "'bond_buyback'",
                     "CREATE PROCEDURE gov_bond_buyback("):
            with self.subTest(item=item): self.assertIn(item,SQL)
        self.assertNotIn("GivePlayerMoney(",UI)
        self.assertNotIn("GivePlayerMoney(",BACKEND)

    def test_coupon_and_maturity_keep_sold_earned_interest(self):
        for item in ("frozen_accrued", "government_bond_buyback_allocations",
                     "p.accrued=p.frozen_accrued+",
                     "INSERT INTO government_bond_claims",
                     "COALESCE(f.frozen,0)-COALESCE(w.collected,0)",
                     "principal_due>0 OR interest_due>0",
                     "v_count + v_boughtback <> v_sold",
                     "v_remaining=v_exposure-v_released",
                     "CREATE PROCEDURE gov_finance_assert_integrity()"):
            with self.subTest(item=item):self.assertIn(item,SQL)

    def test_append_only_and_no_order_mutation(self):
        for tbl in ("government_bond_buybacks","government_bond_buyback_allocations"):
            self.assertIn("CREATE TABLE "+tbl,SQL)
            for verb in ("UPDATE","DELETE"):
                self.assertRegex(SQL,r"CREATE TRIGGER \w+ BEFORE "+verb+r" ON "+tbl)
        self.assertNotIn("UPDATE government_bond_orders SET",SQL)
        self.assertIn("INSERT INTO schema_migrations(version) VALUES (30);",SQL)

    def test_pawn_routing_and_dialog_gates(self):
        for item in ("gBondBuybackReady",
                     "WHERE version=30 LIMIT 1",
                     "stock bool:BondSettlement_Buyback(",
                     "CALL gov_bond_buyback(",
                     "return BondSettlement_Queue(playerid,query,eventKey);"):
            self.assertIn(item,BACKEND)
        for item in ("BONDS_DLG_SELL_QTY (6711)",
                     "BONDS_DLG_SELL_CONFIRM (6712)",
                     "BONDS_DLG_POSITION_ACTION (6713)",
                     "DIALOG_STYLE_INPUT",
                     "gBondsUISellMax[playerid]",
                     "BondSettlement_Buyback(playerid,eventKey",
                     "gBondsUIPendingAction[playerid]=4;",
                     "UNION SELECT issue_id FROM government_bond_claims"):
            with self.subTest(item=item):self.assertIn(item,UI)
        self.assertIn("bonds_boughtback",GOV)
        self.assertIn("030_gov09_bond_buyback.sql",CI)
        self.assertIn("gov09_buyback_mysql_uat.py",CI)

if __name__=="__main__":
    unittest.main()
