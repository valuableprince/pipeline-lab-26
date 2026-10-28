"""GOV-03 structural contracts for protected bond settlement and worker."""
import unittest
from pathlib import Path
from build_contracts import assert_staged

ROOT=Path(__file__).resolve().parents[1]
SQL=(ROOT/"db/migrations/021_bond_settlement.sql").read_text("utf-8")
PAWN=(ROOT/"gamemodes/includes/bond_settlement.inc").read_text("utf-8")
MAIN=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
CI=(ROOT/".github/workflows/build.yml").read_text("utf-8")

class GovernmentBondSettlementContracts(unittest.TestCase):
    def test_schema_escrow_and_claim_receipts(self):
        for s in (
            "ADD COLUMN bond_payout_reserved",
            "bond_fund_reserved + bond_payout_reserved <= balance",
            "CREATE TABLE government_bond_orders",
            "CREATE TABLE government_bond_claims",
            "CREATE TABLE government_bond_claim_receipts",
            "CREATE TABLE bond_payout_ledger",
            "UNIQUE KEY uq_bond_claim_owner (issue_id,account_id)",
            "UNIQUE KEY uq_bond_receipt_event (event_key)",
            "UNIQUE KEY uq_gov_bond_order_event (event_key)",
            "INSERT INTO schema_migrations(version) VALUES (21);",
            "CREATE PROCEDURE gov_bond_buy(",
            "CREATE PROCEDURE gov_bond_maturity_settle(",
            "CREATE PROCEDURE gov_bond_insurance_assess(",
            "CREATE PROCEDURE gov_bond_claim_collect(",
        ):
            with self.subTest(s=s): self.assertIn(s,SQL)
        self.assertNotIn("INSERT INTO accounts",SQL)
        self.assertNotIn("INSERT INTO treasury_accounts",SQL)
        self.assertNotIn("TRUNCATE",SQL)

    def test_atomic_purchase_and_replay(self):
        part=SQL.split("CREATE PROCEDURE gov_bond_buy(",1)[1].split("END$$",1)[0]
        for s in ("START TRANSACTION;","ROLLBACK;","COMMIT;",
                  "FROM treasury_accounts WHERE code='state' FOR UPDATE;",
                  "FROM accounts WHERE account_id=p_account_id FOR UPDATE;",
                  "FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE;",
                  "FROM government_bond_orders WHERE event_key=p_event_key",
                  "v_account_version <> p_expected_version",
                  "v_account_balance < v_payment",
                  "v_sold + p_quantity > v_max_qty",
                  "v_existing_qty+p_quantity > GREATEST(1,v_max_qty DIV 5)",
                  "UPDATE accounts SET balance=balance-v_payment",
                  "UPDATE treasury_accounts SET balance=balance+v_payment",
                  "INSERT INTO government_bond_holdings",
                  "INSERT INTO treasury_ledger",
                  "INSERT INTO government_bond_orders",
                  "INSERT INTO bond_fund_ledger",
                  "'PREMIUM'"):
            with self.subTest(s=s):self.assertIn(s,part)
        self.assertLess(part.index("UPDATE accounts SET"),part.index("INSERT INTO government_bond_orders"))

    def test_pro_rata_maturity_and_insurance_escrow(self):
        m=SQL.split("CREATE PROCEDURE gov_bond_maturity_settle(",1)[1].split("END$$",1)[0]
        i=SQL.split("CREATE PROCEDURE gov_bond_insurance_assess(",1)[1].split("END$$",1)[0]
        for s in ("maturity_at","UTC_TIMESTAMP()","FROM treasury_accounts WHERE code='state' FOR UPDATE;",
                  "v_balance-v_minimum-v_fund-v_pool",
                  "bond_payout_reserved=bond_payout_reserved+v_awarded",
                  "INSERT INTO government_bond_claims",
                  "principal_budget_awarded=",
                  "interest_budget_awarded=",
                  "maturity_event_key=p_event_key"):
            with self.subTest(s=s):self.assertIn(s,m)
        for s in ("DATE_ADD(v_maturity,INTERVAL v_grace SECOND)",
                  "principal_due-principal_budget_awarded",
                  "insurance_awarded=",
                  "bond_fund_reserved=bond_fund_reserved-v_amount",
                  "bond_fund_committed=bond_fund_committed-v_exposure",
                  "bond_payout_reserved=bond_payout_reserved+v_amount",
                  "'INSURANCE_ALLOCATE'",
                  "'INSURANCE'",
                  "released_exposure=max_exposure",
                  "insurance_event_key=p_event_key"):
            with self.subTest(s=s):self.assertIn(s,i)

    def test_no_offline_wallet_overwrite(self):
        collect=SQL.split("CREATE PROCEDURE gov_bond_claim_collect(",1)[1].split("END$$",1)[0]
        for s in ("v_owner<>p_account_id","v_account_version <> p_expected_version",
                  "v_pool<p_amount","v_account_balance>1000000000-p_amount",
                  "UPDATE accounts SET balance=balance+p_amount",
                  "UPDATE treasury_accounts SET balance=balance-p_amount",
                  "bond_payout_reserved=bond_payout_reserved-p_amount",
                  "INSERT INTO government_bond_claim_receipts",
                  "INSERT INTO treasury_ledger","INSERT INTO bond_payout_ledger"):
            with self.subTest(s=s):self.assertIn(s,collect)
        for token in ("stock BondSettlement_Init()","stock BondSettlement_Shutdown()",
                      "stock bool:BondSettlement_Buy(", "stock bool:BondSettlement_Collect(",
                      "stock BondSettlement_OnConnect(playerid)",
                      "stock BondSettlement_OnDisconnect(playerid)",
                      "gGovTransferBusy[playerid] = true",
                      "public BondSettlement_OnPlayerDone(",
                      "public BondSettlement_Tick()",
                      "public BondSettlement_OnScan()",
                      "public BondSettlement_OnAutomated(",
                      "mysql_tquery(gDBPrimary, query, \"BondSettlement_OnPlayerDone\"",
                      "gAuthToken[playerid]!=authToken",
                      "gDBPrimaryVersion[playerid]!=gBondSettlementExpectedVersion[playerid]",
                      "Kick(playerid);",
                      "BondFund_Reload()"):
            with self.subTest(token=token):self.assertIn(token,PAWN)
        self.assertNotIn("Economy_Credit(",PAWN)
        self.assertNotIn("Economy_Debit(",PAWN)
        self.assertNotIn("GivePlayerMoney(",PAWN)
        assert_staged(self,"bond_settlement.inc")
        self.assertIn('#include "bond_settlement.inc"',MAIN)
        self.assertIn("    BondSettlement_Init();",MAIN)
        self.assertIn("    BondSettlement_Shutdown();",MAIN)
        self.assertIn("021_bond_settlement.sql",CI)
        self.assertIn("gov03_mysql_uat.py",CI)

if __name__=="__main__":
    unittest.main()
