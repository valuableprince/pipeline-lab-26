"""Contract tests for the real in-game /bonds purchase and investor portfolio."""
import re
import unittest
from pathlib import Path
from build_contracts import assert_staged

ROOT=Path(__file__).resolve().parents[1]
UI=(ROOT/"gamemodes/includes/bonds_ui.inc").read_text("utf-8")
BACKEND=(ROOT/"gamemodes/includes/bond_settlement.inc").read_text("utf-8")
MAIN=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
COMMANDS=(ROOT/"gamemodes/includes/commands.inc").read_text("utf-8")
SCHEMA=(ROOT/"db/migrations/021_bond_settlement.sql").read_text("utf-8")


class BondInvestorUiContracts(unittest.TestCase):
    def test_route_and_dialog_dispatch(self):
        self.assertRegex(COMMANDS,r"cmd:bonds\(playerid, params\[\]\)")
        self.assertIn("return BondsUI_Main(playerid);",COMMANDS)
        self.assertIn('#include "bonds_ui.inc"',MAIN)
        self.assertIn("BondsUI_OnDialog(playerid, dialogid, response, listitem)",MAIN)
        self.assertIn("BondsUI_Reset(playerid);",MAIN)
        self.assertIn("BondsUI_OnSettlement(playerid, replayed != 0)",BACKEND)
        assert_staged(self,"bonds_ui.inc")

    def test_server_owned_selection_and_sql_read(self):
        self.assertIn("gBondsUIRowIds[playerid][i]=id;",UI)
        self.assertIn("gBondsUIRowIds[playerid][listitem]",UI)
        self.assertIn("i.issue_id=%d AND h.account_id=%d LIMIT 1",UI)
        self.assertIn("h.account_id=%d ORDER BY i.issue_id DESC",UI)
        self.assertIn("status='OFFERING'",UI)
        self.assertIn("offer_start_at<=UTC_TIMESTAMP()",UI)
        self.assertIn("offer_end_at>UTC_TIMESTAMP()",UI)
        self.assertIn("gAuthToken[playerid] == authToken",UI)
        self.assertIn("gDBPrimaryAccountId[playerid] == accountId",UI)
        self.assertIn("gBondsUIQuerySeq[playerid] == requestSeq",UI)
        self.assertIn("mysql_tquery(gDBPrimary, query",UI)
        self.assertNotIn("mysql_query(gDBPrimary, query)",UI)

    def test_buy_one_only_and_sql_amount_authority(self):
        self.assertIn("BondSettlement_Buy(playerid,eventKey,gBondsUISelectedCode[playerid],1)",UI)
        self.assertNotIn("Economy_Debit(",UI)
        self.assertNotIn("Economy_Credit(",UI)
        self.assertNotIn("GivePlayerMoney(",UI)
        self.assertIn("gBondsUIPrice[playerid]",UI)
        self.assertIn("gBondsUIEventSerial++",UI)
        self.assertIn("new eventKey[65];",UI)
        self.assertIn("BondsUI_NewKey(playerid,eventKey,sizeof(eventKey));",UI)
        self.assertIn("SET v_payment=CAST(v_face AS UNSIGNED)*p_quantity",SCHEMA)
        self.assertIn("FROM accounts WHERE account_id=p_account_id FOR UPDATE;",SCHEMA)
        self.assertIn("FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE;",SCHEMA)
        self.assertIn("FROM government_bond_orders WHERE event_key=p_event_key",SCHEMA)

    def test_portfolio_and_escrow_collect(self):
        self.assertIn("FROM government_bond_holdings h INNER JOIN government_bond_issues i",UI)
        self.assertIn("government_bond_claims c",UI)
        self.assertIn("BondsUI_StartCollect(playerid)",UI)
        self.assertIn("gBondsUIClaimId[playerid]",UI)
        self.assertIn("gBondsUIClaimAmount[playerid]",UI)
        self.assertIn("ACCOUNT_MAX_MONEY-gAccountMoney[playerid]",UI)
        self.assertIn("BondSettlement_Collect(playerid,eventKey,",UI)
        self.assertIn("v_owner<>p_account_id",SCHEMA)
        self.assertIn("UPDATE treasury_accounts SET balance=balance-p_amount",SCHEMA)

    def test_dialog_constants_are_separate_and_stable(self):
        constants=re.findall(r"#define\s+BONDS_DLG_\w+\s+\((\d+)\)",UI)
        self.assertEqual(len(constants),7)
        self.assertEqual(len(set(constants)),7)
        self.assertEqual(set(map(int,constants)),set(range(6700,6707)))
        self.assertIn("gBondsUILoading[playerid] || gGovTransferBusy[playerid]",UI)


if __name__=="__main__":
    unittest.main()
