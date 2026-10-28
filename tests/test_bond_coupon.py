"""GOV-07 coupon accrual, offline bookkeeping, and manual collection contracts."""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
SCHEMA=(ROOT/"db/migrations/026_bond_payday_coupons.sql").read_text("utf-8")
SCHEDULER=(ROOT/"gamemodes/includes/bond_coupon.inc").read_text("utf-8")
PAYDAY=(ROOT/"gamemodes/includes/payday.inc").read_text("utf-8")
MAIN=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
UI=(ROOT/"gamemodes/includes/bonds_ui.inc").read_text("utf-8")
SETTLE=(ROOT/"gamemodes/includes/bond_settlement.inc").read_text("utf-8")
CI=(ROOT/".github/workflows/build.yml").read_text("utf-8")

class CouponContracts(unittest.TestCase):
    def test_durable_offline_coupon_entitlements(self):
        for phrase in (
            "CREATE TABLE government_bond_coupon_positions",
            "CREATE TABLE government_bond_coupon_wallets",
            "CREATE TABLE government_bond_coupon_events",
            "CREATE TABLE government_bond_coupon_receipts",
            "PRIMARY KEY (issue_id,account_id)",
            "PRIMARY KEY (issue_id,hour_key)",
            "CREATE PROCEDURE gov_bond_coupon_accrue(",
            "SQL SECURITY DEFINER",
            "FROM government_bond_orders o",
            "p.last_hour_key<p_hour_key",
            "VALUES(accrued)",
            "p_hour_key",
            "ON DUPLICATE KEY UPDATE",
            "UPDATE government_bond_coupon_wallets",
            "v_after-v_before",
        ):
            with self.subTest(phrase=phrase):
                self.assertIn(phrase,SCHEMA)
        self.assertNotIn("FROM players_online",SCHEMA)
        self.assertNotIn("WHERE gAuthState",SCHEMA)
        self.assertIn("IF v_exists>0 THEN",SCHEMA)
        self.assertIn("SET v_replayed=1;",SCHEMA)
        self.assertIn("order_id BIGINT UNSIGNED NOT NULL PRIMARY KEY",SCHEMA)

    def test_hourly_scheduler_processes_offline_in_pages(self):
        self.assertIn('BondCoupon_OnPayday(currentHourKey);',PAYDAY)
        self.assertIn('#include "bond_coupon.inc"',MAIN)
        self.assertIn("BondCoupon_Init();",MAIN)
        self.assertIn("BondCoupon_Shutdown();",MAIN)
        self.assertIn("SELECT version FROM schema_migrations WHERE version=26",SCHEDULER)
        self.assertIn('SetTimer("BondCoupon_Tick",BOND_COUPON_BACKSTOP_MS,true)',SCHEDULER)
        self.assertIn("NOT EXISTS (SELECT 1 FROM government_bond_coupon_events",SCHEDULER)
        self.assertIn("ORDER BY i.issue_id LIMIT %d",SCHEDULER)
        self.assertIn("CALL gov_bond_coupon_accrue(%d,%d)",SCHEDULER)
        self.assertIn("BondCoupon_Tick(); // reconcile at startup",SCHEDULER)
        self.assertNotIn("IsPlayerConnected(",SCHEDULER)
        self.assertNotIn("gAuthState[",SCHEDULER)

    def test_manual_coupon_collection_is_atomic_and_no_double_interest(self):
        for token in (
            "CREATE PROCEDURE gov_bond_coupon_collect(",
            "WHERE code='state' FOR UPDATE",
            "FROM accounts WHERE account_id=p_account_id FOR UPDATE",
            "FROM government_bond_issues WHERE issue_id=p_issue_id FOR UPDATE",
            "FROM government_bond_coupon_wallets",
            "FROM government_bond_coupon_receipts WHERE event_key=p_event_key",
            "v_treasury<v_min+v_fund+v_pool+p_amount",
            "v_account_version<>p_expected_version",
            "p_amount>v_accrued-v_collected-v_converted",
            "SET collected=collected+p_amount",
            "INSERT INTO treasury_ledger",
            "'bond_coupon'",
            "INSERT INTO government_bond_coupon_receipts",
            "v_replayed AS replayed",
            "DROP PROCEDURE IF EXISTS gov_bond_maturity_settle",
            "v_coupon_paid",
            "SET v_interest=v_interest-v_coupon_paid",
            "UPDATE government_bond_coupon_wallets",
            "SET converted=accrued-collected",
            "- COALESCE(w.collected,0)",
        ):
            with self.subTest(token=token):
                self.assertIn(token,SCHEMA)
        self.assertIn("stock bool:BondSettlement_CollectCoupon(",SETTLE)
        self.assertIn("BondSettlement_Queue(playerid,query,eventKey)",SETTLE)
        self.assertNotIn("GivePlayerMoney(",SCHEDULER)
        self.assertIn("guarded reserve", (ROOT/"tests/gov07_coupon_mysql_uat.py").read_text("utf-8"))

    def test_coupon_account_in_bonds_ui(self):
        for token in (
            "#define BONDS_DLG_COUPONS    (6708)",
            "#define BONDS_DLG_COUPON     (6709)",
            "#define BONDS_DLG_COUPON_PAY (6710)",
            "Купонный счёт",
            "BONDS_UI_COUPONS",
            "BONDS_DLG_COUPONS,DIALOG_STYLE_TABLIST_HEADERS",
            "BONDS_DLG_COUPON,DIALOG_STYLE_MSGBOX",
            "BONDS_DLG_COUPON_PAY,DIALOG_STYLE_MSGBOX",
            "w.accrued-w.collected-w.converted AS available",
            "government_bond_coupon_wallets w",
            "gBondsUICouponAmount[playerid]=canGet;",
            "BondSettlement_CollectCoupon(playerid,eventKey,",
            "gBondsUIPendingAction[playerid]=3;",
            "Купоны перечислены на ваш кошелёк",
        ):
            with self.subTest(token=token):
                self.assertIn(token,UI)
        self.assertIn("026_bond_payday_coupons.sql",CI)
        self.assertIn("gov07_coupon_mysql_uat.py",CI)
        self.assertEqual(len(set(re.findall(
            r"#define\s+BONDS_DLG_\w+\s+\((\d+)\)",UI))),11)

if __name__=="__main__":unittest.main()
