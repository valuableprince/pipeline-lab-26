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
        self.assertIn("BondsUI_OnDialog(playerid, dialogid, response, listitem, inputtext)",MAIN)
        self.assertIn("BondsUI_Reset(playerid);",MAIN)
        self.assertIn("BondsUI_OnSettlement(playerid, replayed != 0)",BACKEND)
        assert_staged(self,"bonds_ui.inc")

    def test_server_owned_selection_and_sql_read(self):
        self.assertIn("gBondsUIRowIds[playerid][i]=id;",UI)
        self.assertIn("gBondsUIRowIds[playerid][listitem]",UI)
        self.assertIn("i.issue_id=%d AND (h.issue_id IS NOT NULL OR c.claim_id IS NOT NULL) LIMIT 1",UI)
        self.assertIn("UNION SELECT issue_id FROM government_bond_claims WHERE account_id=%d",UI)
        self.assertIn("status='OFFERING'",UI)
        self.assertIn("offer_start_at<=UTC_TIMESTAMP()",UI)
        self.assertIn("offer_end_at>UTC_TIMESTAMP()",UI)
        self.assertIn("gAuthToken[playerid] == authToken",UI)
        self.assertIn("gDBPrimaryAccountId[playerid] == accountId",UI)
        self.assertIn("gBondsUIQuerySeq[playerid] == requestSeq",UI)
        self.assertIn("mysql_tquery(gDBPrimary, query",UI)
        self.assertNotIn("mysql_query(gDBPrimary, query)",UI)

    def test_batch_quantity_is_user_entered_and_sql_authoritative(self):
        for token in (
            "BONDS_DLG_QUANTITY   (6707)",
            "BONDS_DLG_QUANTITY,DIALOG_STYLE_INPUT",
            "Введите количество облигаций от 1 до %d",
            "stock BondsUI_ShowQuantity(playerid)",
            "if(dialogid==BONDS_DLG_QUANTITY)",
            "gBondsUIStage[playerid]!=BONDS_UI_QUANTITY",
            "inputtext[i]<'0' || inputtext[i]>'9'",
            "count>gBondsUIMaxBuy[playerid]",
            "return BondsUI_StartPurchase(playerid,count)",
            "gBondsUIPurchaseQty[playerid]=quantity",
            "gBondsUIPrice[playerid]*quantity",
            "Итого к оплате:",
            "gBondsUIPendingQty[playerid]=gBondsUIPurchaseQty[playerid]",
            "BondSettlement_Buy(playerid,eventKey,gBondsUISelectedCode[playerid],",
            "gBondsUIPurchaseQty[playerid]))",
            "gBondsUIStage[playerid]=BONDS_UI_NONE;",
            "gBondsUIPendingQty[playerid]=0;",
            "Покупка %d облигаций подтверждена.",
        ):
            with self.subTest(token=token): self.assertIn(token,UI)
        self.assertNotIn("Купить 1",UI)
        self.assertNotIn("BondSettlement_Buy(playerid,eventKey,gBondsUISelectedCode[playerid],1)",UI)
        self.assertNotIn("Economy_Debit(",UI)
        self.assertNotIn("Economy_Credit(",UI)
        self.assertNotIn("GivePlayerMoney(",UI)
        self.assertIn("gBondsUIEventSerial++",UI)
        self.assertIn("BondsUI_NewKey(playerid,eventKey,sizeof(eventKey));",UI)
        self.assertIn("SET v_payment=CAST(v_face AS UNSIGNED)*p_quantity",SCHEMA)
        self.assertIn("FROM accounts WHERE account_id=p_account_id FOR UPDATE;",SCHEMA)
        self.assertIn("FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE;",SCHEMA)
        self.assertIn("FROM government_bond_orders WHERE event_key=p_event_key",SCHEMA)

    def test_strict_per_issue_twenty_percent_and_cross_issue_portfolio(self):
        for token in (
            "LEFT JOIN government_bond_holdings h ON h.issue_id=i.issue_id AND h.account_id=%d",
            "i.max_quantity DIV 5 AS per_account_limit",
            "COALESCE(h.quantity,0) AS owned",
            "i.max_quantity-i.sold_quantity AS remaining",
            "LEAST(max_transaction,1000000000)",
            "new capacity=holdLimit-owned;",
            "new affordable=gAccountMoney[playerid]/face;",
            "if(maxBuy>capacity)maxBuy=capacity;",
            "if(maxBuy>affordable)maxBuy=affordable;",
            "if(maxBuy>purchaseCap/face)maxBuy=purchaseCap/face;",
            "gBondsUIMaxBuy[playerid]=maxBuy;",
            "gBondsUIHoldLimit[playerid]=holdLimit;",
            "gBondsUIOwned[playerid]=owned;",
            "gBondsUISelectedIssue[playerid] = issueId;",
            "UNION SELECT issue_id FROM government_bond_claims WHERE account_id=%d",
        ):
            with self.subTest(token=token): self.assertIn(token,UI)
        self.assertIn("v_existing_qty+p_quantity > v_max_qty DIV 5",SCHEMA)
        self.assertNotIn("GREATEST(1,v_max_qty DIV 5)",SCHEMA)
        self.assertIn("PRIMARY KEY (issue_id, account_id)",
            (ROOT/"db/migrations/020_government_bond_issues.sql").read_text("utf-8"))

    def test_portfolio_debt_statuses_are_localized_without_changing_sql(self):
        issue_schema=(ROOT/"db/migrations/020_government_bond_issues.sql").read_text("utf-8")
        allowed=set(re.findall(
            r"'([A-Z_]+)'",
            re.search(r"chk_government_bond_debt CHECK\s*\(debt_status IN \((.*?)\)\)",issue_schema,re.S).group(1)))
        expected={
            "NOT_DUE":"Срок не наступил",
            "DUE":"К погашению",
            "PARTIALLY_PAID":"Частично погашен",
            "OVERDUE":"Просрочен",
            "DEFAULTED":"Дефолт",
            "RESTRUCTURED":"Реструктурирован",
            "SETTLED":"Погашен полностью",
        }
        self.assertEqual(allowed,set(expected))
        label_map=UI.split("stock BondsUI_DebtStatusLabel(",1)[1].split(
            "new gBondsUIStage",1)[0]
        for code,label in expected.items():
            self.assertIn(f'!strcmp(status,"{code}")) format(output,size,"{label}")',label_map)
        self.assertIn("BondsUI_DebtStatusLabel(debt,debtLabel,sizeof(debtLabel));",UI)
        self.assertIn("rate/100,rate%100,debtColor,debtLabel,remaining,available",UI)
        self.assertNotIn("debtColor,debt,remaining,available",UI)
        self.assertIn('!strcmp(debt,"DEFAULTED",true)',UI)
        self.assertIn('!strcmp(debt,"OVERDUE",true)',UI)
        self.assertIn('!strcmp(debt,"SETTLED",true)',UI)

    def test_portfolio_and_escrow_collect(self):
        self.assertIn("UNION SELECT issue_id FROM government_bond_claims WHERE account_id=%d",UI)
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
        self.assertIn(r"{E8EDF3}$%d\n",UI)
        self.assertIn(r"{E8EDF3}$0\n",UI)
        self.assertIn("new rows=0, body[3500],line[260]",UI)
        # Every data row is assembled with three tabs and a newline.
        rows = re.findall(r'format\(line,sizeof\(line\),\s*"(.*?)"', UI, re.S)
        self.assertEqual(len(rows), 4, "Offer, coupon and portfolio rows must all be tested")
        for row in rows:
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

    def test_monochrome_bonds_ui_and_risk_visibility(self):
        palette=set(re.findall(r"\{[0-9A-F]{6}\}",UI))
        self.assertEqual(palette,{"{E8EDF3}"})
        self.assertIn("Риск дефолта: доход не застрахован.",UI)
        self.assertIn("Проценты не застрахованы.",UI)

    def test_zero_buy_limit_has_precise_reason_and_no_purchase_button(self):
        for token in (
            "AS offer_open",
            "Достигнут лимит владения: 20%% выпуска.",
            "Все облигации этого выпуска распроданы.",
            "Недостаточно денег для покупки одной облигации.",
            "Размещение закрыто либо ещё не началось.",
            "Лимит одной операции казны меньше номинала.",
            'maxBuy>0?"Купить":"Назад"',
            'maxBuy>0?"Назад":"Закрыть"',
            'if(!response || gBondsUIMaxBuy[playerid]<1)',
            "if(!offerOpen)maxBuy=0;",
            "return BondsUI_LoadDetail(playerid,BONDS_UI_OFFER,gBondsUISelectedIssue[playerid]);",
        ):
            with self.subTest(token=token):
                self.assertIn(token,UI)
        self.assertNotIn("Покупка недоступна: исчерпан лимит 20%, остаток или баланс.",UI)
        self.assertIn("gov_bond_buy",SCHEMA)

    def test_dialog_constants_are_separate_and_stable(self):
        constants=re.findall(r"#define\s+BONDS_DLG_\w+\s+\((\d+)\)",UI)
        self.assertEqual(len(constants),16)
        self.assertEqual(len(set(constants)),16)
        self.assertEqual(set(map(int,constants)),set(range(6700,6716)))
        self.assertIn("gBondsUILoading[playerid] || gGovTransferBusy[playerid]",UI)


    def test_completed_claims_move_to_read_only_history(self):
        self.assertIn("WHERE c.claim_id IS NULL OR c.amount_collected<c.principal_due+c.interest_due",UI)
        self.assertIn("c.amount_collected>=c.principal_due+c.interest_due",UI)
        self.assertIn("i.maturity_event_key IS NOT NULL",UI)
        self.assertIn("BONDS_DLG_HISTORY,DIALOG_STYLE_TABLIST_HEADERS",UI)
        self.assertIn("BONDS_DLG_HISTORY_DETAIL,DIALOG_STYLE_MSGBOX",UI)
        self.assertIn("case 5:return BondsUI_LoadList(playerid,BONDS_UI_HISTORY);",UI)
        self.assertIn("if(dialogid==BONDS_DLG_HISTORY_DETAIL)",UI)
        self.assertIn("Все выплаты получены. Запись сохранена в финансовой истории.",UI)
        self.assertNotIn("DELETE FROM government_bond_claims",UI)
        self.assertNotIn("UPDATE government_bond_claims",UI)

    def test_maturity_notices_are_commit_confirmed_and_login_safe(self):
        self.assertIn("BondsUI_NotifyMaturityOnline(issueId);",BACKEND)
        self.assertIn("BondsUI_NotifyMaturityLogin(playerid);",MAIN)
        self.assertIn("i.maturity_event_key IS NOT NULL",UI)
        self.assertIn("c.principal_budget_awarded+c.interest_budget_awarded+c.insurance_awarded>c.amount_collected",UI)
        self.assertIn("gAuthToken[playerid]!=authToken",UI)
        self.assertIn("gDBPrimaryAccountId[playerid]!=accountId",UI)
        self.assertIn("gBondsUIMaturitySeq[playerid]!=seq",UI)
        self.assertIn("Минфин: по %s доступна выплата $%d.",UI)
        self.assertIn("Минфин: по %s ожидает выплата $%d.",UI)

    def test_claim_receipt_confirmation_references_paid_amount(self):
        self.assertIn("gBondsUIPendingPayAmount[playerid]=gBondsUIClaimAmount[playerid];",UI)
        self.assertIn('format(gBondsUIPendingPayCode[playerid],65,"%s",gBondsUISelectedCode[playerid]);',UI)
        self.assertIn("Минфин перечислил $%d по облигациям %s.",UI)
        self.assertIn("После полного расчёта выпуск перейдёт",UI)
        self.assertIn("gBondsUIPendingPayAmount[playerid]=0;",UI)



if __name__=="__main__":
    unittest.main()
