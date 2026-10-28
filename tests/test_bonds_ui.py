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

    def test_tabular_lists_have_four_colored_headers_and_true_tabs(self):
        # TABLIST_HEADERS consumes its first row as headers, not listitem=0.
        self.assertIn("BONDS_DLG_OFFERS,DIALOG_STYLE_TABLIST_HEADERS",UI)
        self.assertIn("BONDS_DLG_PORTFOLIO,DIALOG_STYLE_TABLIST_HEADERS",UI)
        self.assertNotIn("BONDS_DLG_OFFERS,DIALOG_STYLE_LIST",UI)
        self.assertNotIn("BONDS_DLG_PORTFOLIO,DIALOG_STYLE_LIST",UI)
        self.assertIn(r"Выпуск\tНоминал\tДоходность\tДоступно\n",UI)
        self.assertIn(r"Выпуск\tКоличество\tНоминал\tК выплате\n",UI)
        self.assertIn(r"%d.%02d%%\t",UI)
        self.assertIn(r"{78D8AA}$%d\n",UI)
        self.assertIn(r"{8795A5}$0\n",UI)
        self.assertIn("new rows=0, body[3500],line[260]",UI)
        # Every data row is assembled with three tabs and a newline.
        for row in re.findall(r'format\\(line,sizeof\\(line),\\s*"(.*?)"',UI,re.S):
            self.assertEqual(row.count(r"\t"),3,row)
            self.assertTrue(row.endswith(r"\n"),row)

    def test_empty_rows_and_pagination_keep_selection_indices_stable(self):
        self.assertIn("if (shown==0)",UI)
        self.assertIn("if(listitem==0)return 1; // empty state is informational",UI)
        self.assertIn("index=1;",UI)
        self.assertIn("new index=gBondsUIListCount[playerid];",UI)
        self.assertIn("if(listitem>=0 && listitem<gBondsUIListCount[playerid])",UI)
        self.assertIn("gBondsUIRowIds[playerid][listitem]",UI)
        self.assertIn(r"Следующая страница\t\t\t\n",UI)
        self.assertIn(r"Предыдущая страница\t\t\t\n",UI)
        self.assertIn(r"Вернуться в меню\t\t\t\n",UI)
        self.assertIn("gBondsUIPage[playerid]+1",UI)
        self.assertIn("BONDS_UI_PAGE_SIZE   (10)",UI)

    def test_premium_color_scheme_and_risk_visibility(self):
        for color in ("{D9B872}","{DDBB77}","{E9C67E}",
                      "{78D8AA}","{E48A8A}","{B8C6D7}","{8795A5}"):
            self.assertIn(color,UI)
        self.assertIn("Риск дефолта: доход не застрахован.",UI)
        self.assertIn("Проценты не застрахованы.",UI)
        self.assertIn("new debtColor[9];",UI)

    def test_dialog_constants_are_separate_and_stable(self):
        constants=re.findall(r"#define\s+BONDS_DLG_\w+\s+\((\d+)\)",UI)
        self.assertEqual(len(constants),7)
        self.assertEqual(len(set(constants)),7)
        self.assertEqual(set(map(int,constants)),set(range(6700,6707)))
        self.assertIn("gBondsUILoading[playerid] || gGovTransferBusy[playerid]",UI)


if __name__=="__main__":
    unittest.main()
