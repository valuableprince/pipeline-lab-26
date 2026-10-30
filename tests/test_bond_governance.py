"""GOV-05 official console, automatic issue codes and role separation contracts."""
from pathlib import Path
import re
import unittest
from build_contracts import assert_staged

ROOT=Path(__file__).resolve().parents[1]
SQL=(ROOT/"db/migrations/023_bond_governance.sql").read_text("utf-8")
UI=(ROOT/"gamemodes/includes/bond_governance_ui.inc").read_text("utf-8")
EDU=(ROOT/"gamemodes/includes/bond_education.inc").read_text("utf-8")
MAIN=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
COMMANDS=(ROOT/"gamemodes/includes/commands.inc").read_text("utf-8")
DB=(ROOT/"gamemodes/includes/db_primary.inc").read_text("utf-8")
CI=(ROOT/".github/workflows/build.yml").read_text("utf-8")


class BondGovernanceContracts(unittest.TestCase):
    def test_schema_and_atomic_global_sequence(self):
        for phrase in ("CREATE TABLE government_bond_code_sequences",
            "CREATE TABLE government_bond_tax_earmarks",
            "CREATE PROCEDURE gov_bond_tax_earmark(",
            "CREATE TRIGGER government_bond_code_immutable",
            "CREATE PROCEDURE gov_bond_issue_create_auto(",
            "CREATE PROCEDURE gov_bond_official_approve(",
            "CREATE PROCEDURE gov_bond_official_open(",
            "CREATE PROCEDURE gov_bond_official_cancel(",
            "CREATE PROCEDURE gov_bond_official_reject(",
            "INSERT INTO schema_migrations(version) VALUES (23);"):
            with self.subTest(phrase=phrase):self.assertIn(phrase,SQL)
        create=SQL.split("CREATE PROCEDURE gov_bond_issue_create_auto(",1)[1].split("END$$",1)[0]
        for phrase in ("START TRANSACTION;","ROLLBACK;","COMMIT;",
          "f.term_end>UNIX_TIMESTAMP() FOR UPDATE","v_rank NOT IN (2,3,4)",
          "FROM government_bond_issues WHERE create_event_key=p_event_key",
          "FROM government_bond_code_sequences WHERE sequence_id=1 FOR UPDATE",
          "SET v_number=v_number+1",
          "SET v_code=CONCAT('GOV-',LPAD(v_number,6,'0'))",
          "INSERT INTO government_bond_issues",
          "INSERT INTO government_bond_issue_audit",
          "DATE_ADD(v_start_new,INTERVAL p_offer_days DAY)"):
            with self.subTest(phrase=phrase):self.assertIn(phrase,create)
        self.assertIn("CONSTRAINT chk_bond_sequence_singleton CHECK (sequence_id = 1)",SQL)
        self.assertIn("CONSTRAINT chk_bond_sequence_max CHECK (last_number <= 999999)",SQL)
        self.assertIn("SELECT 1, COALESCE(MAX(CAST(SUBSTRING(issue_code,5) AS UNSIGNED)),0)",SQL)
        self.assertNotIn("issue_year",SQL)
        self.assertNotIn("v_year",create)
        self.assertNotIn("YEAR(v_start_new)",create)
        self.assertIn("TAX_EARMARK",SQL)
        self.assertIn("v_amount=(CAST(v_tax_amount AS UNSIGNED)*v_rate) DIV 10000",SQL)
        self.assertIn("v_balance-v_min-v_fund-v_payout",SQL)
        self.assertIn("tax_ledger_id BIGINT UNSIGNED NOT NULL PRIMARY KEY",SQL)
        self.assertNotIn("UPDATE treasury_accounts",create)
        self.assertNotIn("UPDATE accounts SET balance",create)

    def test_issue_and_crisis_statuses_are_localized_at_display_only(self):
        issue_schema=(ROOT/"db/migrations/020_government_bond_issues.sql").read_text("utf-8")
        crisis_schema=(ROOT/"db/migrations/022_bond_debt_crisis.sql").read_text("utf-8")
        issue_codes=set(re.findall(
            r"'([A-Z_]+)'",
            re.search(r"chk_government_bond_status CHECK\s*\(status IN \((.*?)\)\)",issue_schema,re.S).group(1)))
        crisis_codes=set(re.findall(
            r"'([A-Z_]+)'",
            re.search(r"chk_bond_crisis_state CHECK\s*\(bond_crisis_state IN \((.*?)\)\)",crisis_schema,re.S).group(1)))
        issue_map=UI.split("stock GovBond_UI_IssueStatusLabel(",1)[1].split(
            "stock GovBond_UI_CrisisStatusLabel(",1)[0]
        crisis_map=UI.split("stock GovBond_UI_CrisisStatusLabel(",1)[1].split(
            "new bool:gGovUIReady;",1)[0]
        expected_issues={
            "DRAFT":"На подпись","APPROVED":"Утверждён",
            "OFFERING":"Размещается","CLOSED":"Размещение закрыто",
            "CANCELLED":"Отменён","REJECTED":"Отклонён"
        }
        expected_crisis={
            "NORMAL":"Норма","STRESSED":"Напряжённый",
            "DEFAULT":"Дефолт","RECOVERY":"Восстановление"
        }
        self.assertEqual(issue_codes,set(expected_issues))
        self.assertEqual(crisis_codes,set(expected_crisis))
        for code,label in expected_issues.items():
            self.assertIn(f'!strcmp(status,"{code}")) format(output,size,"{label}")',issue_map)
        for code,label in expected_crisis.items():
            self.assertIn(f'!strcmp(status,"{code}")) format(output,size,"{label}")',crisis_map)
        self.assertIn("GovBond_UI_IssueStatusLabel(status,statusLabel,sizeof(statusLabel));",UI)
        self.assertEqual(UI.count("GovBond_UI_IssueStatusLabel(status,statusLabel,sizeof(statusLabel));"),2)
        self.assertIn("code,applicant,quantity,face,statusLabel);",UI)
        self.assertIn("code,applicant,minister,statusLabel,title,purpose",UI)
        self.assertIn("format(gGovUISelectedStatus[playerid],20,\"%s\",status);",UI)
        self.assertIn("GovBond_UI_CrisisStatusLabel(crisis,crisisLabel,sizeof(crisisLabel));",UI)
        self.assertIn("cash,fund,committed,payout,crisisLabel,issues",UI)

    def test_percentage_entry_and_complete_signatory_card(self):
        self.assertIn("stock bool:GovBond_UI_ParsePercent(const input[],&basisPoints)",UI)
        self.assertIn("GovBond_UI_ParsePercent(inputtext,num)",UI)
        self.assertIn("basisPoints=whole*100+fraction;",UI)
        self.assertIn("fractionDigits>2",UI)
        self.assertIn("От 0 до 100%%",UI)
        self.assertNotIn("Доходность за весь срок (базисные пункты)",UI)
        for marker in ("i.issue_type","typeLabel","offer_days","maturity_days",
                       "Тип займа:", "Размещение:", "Погашение:",
                       "GovBond_UI_TypeLabel(issueType,typeLabel,sizeof(typeLabel))"):
            with self.subTest(marker=marker):self.assertIn(marker,UI)

    def test_returned_issues_are_auditable_and_resubmittable(self):
        revision=(ROOT/"db/migrations/025_bond_application_revisions.sql").read_text("utf-8")
        for marker in (
            "status IN ('DRAFT','RETURNED','APPROVED','OFFERING'",
            "CREATE TABLE government_bond_revision_reviews",
            "gov_bond_revision_reviews_upd",
            "CREATE PROCEDURE gov_bond_official_return(",
            "CREATE PROCEDURE gov_bond_issue_resubmit(",
            "v_status<>'DRAFT'", "v_status<>'RETURNED'",
            "v_creator<>p_actor",
            "revision_count=revision_count+1",
            "SET status='RETURNED'",
            "SET status='DRAFT'",
            "RETURN','RESUBMIT",
            "END$",
        ):
            with self.subTest(marker=marker):self.assertIn(marker,revision)
        self.assertNotIn("END$ AND",revision)
        self.assertNotIn("END$ THEN",revision)
        self.assertIn("GovBond_UI_ReturnReason(playerid)",UI)
        self.assertIn("GovBond_UI_Resubmit(playerid)",UI)
        self.assertIn('!strcmp(status,"RETURNED")',UI)
        self.assertIn("creatorId==gDBPrimaryAccountId[playerid]",UI)
        self.assertIn('gGovUIStage[playerid]==13',UI)
        self.assertIn("CALL gov_bond_issue_resubmit(",UI)
        self.assertIn("CALL gov_bond_official_return(",UI)
        self.assertIn("На доработке",UI)
        self.assertIn("gov05_revision_mysql_uat.py",CI)
        self.assertIn("025_bond_application_revisions.sql",CI)

    def test_offering_is_rebased_on_real_opening_not_draft(self):
        revision=(ROOT/"db/migrations/025_bond_application_revisions.sql").read_text("utf-8")
        opening=revision.split("CREATE PROCEDURE gov_bond_issue_open(",1)[1].split("END$",1)[0]
        approval=revision.split("CREATE PROCEDURE gov_bond_issue_approve(",1)[1].split("END$",1)[0]
        self.assertIn("offer_start_at=UTC_TIMESTAMP()",opening)
        self.assertIn("INTERVAL v_offer_days DAY",opening)
        self.assertIn("INTERVAL v_maturity_days DAY",opening)
        self.assertIn("SELECT issue_id,status,offer_start_at,offer_end_at,maturity_at",opening)
        self.assertIn("SQL SECURITY DEFINER",opening)
        self.assertIn("SQL SECURITY DEFINER",approval)
        self.assertIn("OR (v_end <= UTC_TIMESTAMP()",approval)
        self.assertIn("AND BINARY p_issue_code NOT REGEXP",approval)

    def test_ministry_approval_requires_explicit_signature_for_every_issue(self):
        block=SQL.split("CREATE PROCEDURE gov_bond_official_approve(",1)[1].split("END$$",1)[0]
        self.assertIn("faction_members",block)
        self.assertIn("m.faction_id=5",block)
        self.assertIn("v_rank NOT IN (3,4)",block)
        self.assertIn("CALL gov_bond_issue_approve(p_event_key,p_issue_code,p_actor)",block)
        self.assertIn("INSERT INTO factions(faction_id,is_enabled,term_start,term_end)",SQL)
        self.assertNotIn("government_bond_officials",SQL)
        self.assertNotIn("gov_bond_role_set",SQL)
        reject=SQL.split("CREATE PROCEDURE gov_bond_official_reject(",1)[1].split("END$$",1)[0]
        for phrase in ("TRIM(p_reason)","v_rank NOT IN (3,4)",
            "v_status<>'DRAFT'","rejection_reason=p_reason",
            "SET status='REJECTED'","'REJECT'","'DRAFT','REJECTED'",
            "START TRANSACTION;","FOR UPDATE;","COMMIT;","v_replayed AS replayed"):
            with self.subTest(phrase=phrase):self.assertIn(phrase,reject)
        self.assertNotIn("UPDATE treasury_accounts SET",reject)
        self.assertIn("('CREATE','APPROVE','OPEN','CANCEL','REJECT')",
            (ROOT/"db/migrations/020_government_bond_issues.sql").read_text("utf8"))
        core=(ROOT/"db/migrations/020_government_bond_issues.sql").read_text("utf-8")
        self.assertIn("v_creator=p_actor_id AND NOT EXISTS",core)
        self.assertIn("m.rank_id IN (3,4)",core)

    def test_gov_console_is_sql_backed(self):
        for phrase in ("stock GovBond_UI_Init()","stock GovBond_UI_Main(playerid)",
          "stock GovBond_UI_Wizard(playerid,step)",
          "stock GovBond_UI_Report(playerid)",
          "stock GovBond_UI_PublicReport(playerid)",
          "public GovBond_UI_TaxTick()",
          "public GovBond_UI_OnTaxScan()",
          "public GovBond_UI_OnTaxPost()",
          "gov_bond_tax_earmark",
          "stock GovBond_UI_LoadIssues(playerid,page=0)",
          "stock GovBond_UI_LoadIssue(playerid,issueId)",
          "GovBond_UI_OnDialog(playerid,dialogid,response,listitem,const inputtext[])",
          "gDBPrimaryAccountId[playerid]",
          "gAuthToken[playerid]!=authToken",
          "gGovUIReadSerial[playerid]!=serial",
          "gov_bond_issue_create_auto",
          "gov_bond_official_approve",
          "gov_bond_official_open",
          "gov_bond_official_cancel",
          "faction_members",
          "finance_rank",
          "Leader_PublicRank(FACTION_FINANCE,internalRank)",
          "Leader_FinanceAuthorized(playerid,3)",
          "stock GovBond_UI_RejectReason(playerid)",
          "stock GovBond_UI_Reject(playerid,const reason[])",
          "gov_bond_official_reject",
          "DIALOG_STYLE_INPUT",
          "Подписать","Отклонить",
          "rejection_reason",
          "creator_account_id",
          "Причина отказа:",
          "minister",
          "applicant",
          "[Минфин] %s подал заявку на подпись выпуска %s.",
          "[Минфин] Для подписи откройте /bondgov -> Реестр выпусков (%s).", 
          "bond_crisis_state",
          "bond_fund_reserved",
          "bond_payout_reserved",
          "government_bond_claims",
          "GOVUI_LIST,DIALOG_STYLE_TABLIST_HEADERS",
          "GOVUI_CONFIRM,DIALOG_STYLE_MSGBOX"):
            with self.subTest(phrase=phrase):self.assertIn(phrase,UI)
        self.assertNotIn('cmd:bondsign(',COMMANDS)
        self.assertNotIn('GovBond_UI_Sign(',UI)
        self.assertIn('GOVUI_LIST,DIALOG_STYLE_TABLIST_HEADERS',UI)
        self.assertIn('GOVUI_ISSUE,DIALOG_STYLE_MSGBOX',UI)
        self.assertIn('if(gGovUIStage[playerid]==11)',UI)
        self.assertIn('return GovBond_UI_Reject(playerid,inputtext);',UI)
        self.assertNotIn("bond:gov5:autoapprove:",UI)
        self.assertIn("if (replayed==0 && Leader_PlayerOrg(playerid)==FACTION_FINANCE)",UI)
        self.assertIn("Leader_PlayerOrg(i)==FACTION_FINANCE",UI)
        self.assertIn('cmd:bondgov(playerid, params[])',COMMANDS)
        self.assertNotIn("cmd:bondgovrole(",COMMANDS)
        self.assertIn('#include "bond_governance_ui.inc"',MAIN)
        self.assertIn("    GovBond_UI_Init();",MAIN)
        self.assertIn("    GovBond_UI_Shutdown();",MAIN)
        self.assertEqual(MAIN.count("    GovBond_UI_Reset(playerid);"),2)
        self.assertIn("GovBond_UI_OnDialog(playerid, dialogid, response, listitem, inputtext)",MAIN)
        self.assertIn("GovBond_UI_OnError(errorid, callback, query)",DB)
        self.assertIn("023_bond_governance.sql",CI)
        self.assertIn("gov05_mysql_uat.py",CI)
        self.assertIn("Публичная отчётность казны",
            (ROOT/"gamemodes/includes/bonds_ui.inc").read_text("utf-8"))
        self.assertNotIn("GivePlayerMoney(",UI)
        assert_staged(self,"bond_governance_ui.inc")

    def test_public_offering_announcement_only_after_first_sql_commit(self):
        block=UI.split("public GovBond_UI_OnWrite(",1)[1].split(
            "// Every successful tax credit",1)[0]
        init=UI.split("stock bool:GovBond_UI_Write(",1)[1].split(
            "stock GovBond_UI_Main(",1)[0]
        for term in (
            "gGovUIPendingIssue[playerid]=gGovUISelectedIssue[playerid]",
            'format(gGovUIPendingCode[playerid],65,"%s",gGovUISelectedCode[playerid]);',
        ):
            self.assertIn(term,init)
        for term in (
            'if (valid && action==3)',
            'cache_get_value_name_int(0,"issue_id",issueId)',
            'cache_get_value_name(0,"issue_status",decisionStatus,sizeof(decisionStatus))',
            'issueId==gGovUIPendingIssue[playerid]',
            'replayed==1 || !strcmp(decisionStatus,"OFFERING")',
            'if (!valid || (replayed!=0 && replayed!=1))',
            'if (action==3 && replayed==0)',
            'SendClientMessageToAll(COLOR_AMBER,notice);',
            'SendClientMessageToAll(FACTION_COLOR,',
            '[Министерство финансов] Открыто размещение облигаций {FFFFFF}%s{D9AA55}.',
            'gGovUIPendingCode[playerid]);',
            '[Министерство финансов] Облигации доступны для покупки: {FFFFFF}/bonds.',
        ):
            with self.subTest(term=term): self.assertIn(term,block)
        self.assertNotIn("[Госзаймы]",UI)
        self.assertEqual(UI.count("SendClientMessageToAll("),2)
        self.assertIn("gGovUIPendingIssue[playerid]=0;",block)
        self.assertIn("gGovUIPendingCode[playerid][0]=EOS;",block)
        self.assertLess(block.index("if (!valid || (replayed!=0 && replayed!=1))"),
                        block.index("if (action==3 && replayed==0)"))
        opening=(ROOT/"db/migrations/020_government_bond_issues.sql").read_text("utf-8")
        opening=opening.split("CREATE PROCEDURE gov_bond_issue_open(",1)[1].split("END$",1)[0]
        self.assertIn("IF v_status <> 'APPROVED'",opening)
        self.assertIn("SET status='OFFERING'",opening)
        self.assertIn("v_status AS issue_status,v_replayed AS replayed",opening)

    def test_approval_denial_explains_fund_minimum_and_missing_amount(self):
        self.assertIn("stock bool:GovBond_UI_LoadApprovalReason(playerid)",UI)
        self.assertIn("GovBond_UI_Query(playerid,query,5)",UI)
        self.assertIn('if (kind==5)',UI)
        self.assertIn("Нужно минимум: $%s",UI)
        self.assertIn("Сейчас в резерве: $%s",UI)
        self.assertIn("Не хватает: $%s",UI)
        self.assertIn("Страховой фонд выключен.",UI)
        self.assertIn("только заместитель или министр финансов",UI)
        self.assertIn("Выпуск уже подписан или больше не ожидает подписи",UI)
        self.assertIn("CEIL(CAST(i.face_value AS DECIMAL(20,0))*t.bond_fund_coverage_bps/10000)*i.max_quantity AS exposure",UI)
        self.assertIn("CAST(CEIL((x.committed+x.exposure)*10000/NULLIF(x.util_bps,0)) AS CHAR) AS reserve_min",UI)
        self.assertIn("CAST(GREATEST(0,CEIL((x.committed+x.exposure)*10000/NULLIF(x.util_bps,0))-x.reserved) AS CHAR) AS reserve_short",UI)
        self.assertIn('if(failedAction==2)',UI)
        self.assertIn('GOVUI_APPROVAL_REASON,DIALOG_STYLE_MSGBOX',UI)
        self.assertIn("if(dialogid==GOVUI_APPROVAL_REASON)",UI)
        self.assertIn("if(gGovUIStage[playerid]!=13)",UI)
        self.assertNotIn("Казначейство отклонило решение: проверяйте роль, статус, фонд и подписи.",UI)
        # Explanation is read-only. The protected approval routine is unchanged.
        self.assertIn("gov_bond_official_approve",UI)
        self.assertNotIn("UPDATE treasury_accounts SET",UI)

    def test_minister_insurance_fund_flow_and_faction_notice(self):
        for phrase in (
            "GOVUI_FUND_AMOUNT (6809)",
            "GOVUI_FUND_CONFIRM (6810)",
            "Пополнить страховой фонд (министр)",
            "gGovUIRole[playerid]!=4",
            "Leader_FinanceAuthorized(playerid,4)",
            "gGovUIStage[playerid]=14;",
            "gGovUIStage[playerid]=15;",
            "if(dialogid==GOVUI_FUND_AMOUNT)",
            "if(dialogid==GOVUI_FUND_CONFIRM)",
            "GovBond_UI_ParsePositive(inputtext,amount,gGovUIFundMax[playerid])",
            "fund:gov5:%d:%d:%d:%d",
            "CALL gov_bond_official_fund_capitalize",
            "if(valid && action==8)",
            "gGovUIPendingFundAmount[playerid]",
            'if(replayed==0)',
            "Leader_PlayerOrg(i)==FACTION_FINANCE",
            "пополнил страховой фонд на $%d. Резерв: $%d.",
            "BondFund_Reload()",
            'SELECT version FROM schema_migrations WHERE version=29 LIMIT 1',
        ):
            with self.subTest(phrase=phrase):
                self.assertIn(phrase,UI)
        self.assertIn("gGovUIStage[playerid]=0; // consume before SQL request",UI)
        self.assertNotIn("UPDATE treasury_accounts SET",UI)
        sql=(ROOT/"db/migrations/029_minfin_fund_capitalization.sql").read_text("utf-8")
        self.assertIn("SQL SECURITY DEFINER",sql)
        self.assertIn("m.rank_id=4",sql)
        self.assertIn("f.term_end>UNIX_TIMESTAMP()",sql)
        self.assertIn("WHERE code='state' FOR UPDATE",sql)
        self.assertIn("UPDATE treasury_accounts SET",sql)
        self.assertIn("INSERT INTO bond_fund_ledger",sql)
        self.assertIn("v_balance-v_minimum-v_reserved<p_amount",sql)
        self.assertIn("INSERT INTO schema_migrations(version) VALUES (29)",sql)
        self.assertIn("gov08_fund_mysql_uat.py",CI)
        self.assertNotIn("GivePlayerMoney(",UI)

    def test_cancel_loan_type_returns_to_treasury_main(self):
        # Manual UAT 3.1: Back on loan-type selection must not close /bondgov.
        block=UI.split("if(dialogid==GOVUI_TYPE)",1)[1].split(
            "if(dialogid==GOVUI_INPUT)",1)[0]
        self.assertIn("if(gGovUIStage[playerid]!=9)return 1;",block)
        self.assertIn("if(!response)return GovBond_UI_Main(playerid);",block)
        self.assertIn("if(listitem<0 || listitem>2)return 1;",block)
        self.assertLess(block.index("if(!response)"),block.index("switch(listitem)"))
        self.assertNotIn("|| !response ||",block)

    def test_expired_offering_is_not_labelled_as_active(self):
        # A stored OFFERING state may persist until the maturity assessment.
        # Read-only presentation must respect the actual offer_end_at window.
        self.assertEqual(UI.count(
            "(i.status='OFFERING' AND i.offer_end_at<=UTC_TIMESTAMP()) AS offering_expired"),2)
        self.assertEqual(UI.count('cache_get_value_name_int(i,"offering_expired",expired)'),1)
        self.assertEqual(UI.count('cache_get_value_name_int(0,"offering_expired",expired)'),1)
        self.assertEqual(UI.count(
            'if(expired)format(statusLabel,sizeof(statusLabel),"Размещение завершено");'),2)
        self.assertEqual(UI.count(
            "WHERE status='OFFERING' AND offer_start_at<=UTC_TIMESTAMP() AND offer_end_at>UTC_TIMESTAMP()) AS offering"),2)
        # Keep the true SQL state for permission checks and payment processing.
        self.assertIn('format(gGovUISelectedStatus[playerid],20,"%s",status);',UI)
        self.assertNotIn("UPDATE government_bond_issues SET",UI)

    def test_bond_term_day_forms_are_grammatically_correct(self):
        # 1 день, 2 дня, 5 дней, 11 дней, 21 день, etc.
        block=UI.split("stock GovBond_UI_DayWord(",1)[1].split(
            "stock GovBond_UI_TypeLabel(",1)[0]
        for marker in (
            "lastTwo=days%100,last=days%10",
            "lastTwo>=11 && lastTwo<=14",
            'format(output,size,"дней")',
            'last==1',
            'format(output,size,"день")',
            'last>=2 && last<=4',
            'format(output,size,"дня")',
        ):
            self.assertIn(marker,block)
        self.assertIn("Размещение: {E8EDF3}%d %s",UI)
        self.assertIn("Погашение: {E8EDF3}через %d %s после размещения",UI)
        self.assertIn("Размещение: {FFD15C}%d %s",UI)
        self.assertIn("Погашение через: {FFD15C}%d %s после размещения",UI)
        self.assertIn("offerDays,offerDayWord,maturityDays,maturityDayWord",UI)
        self.assertIn("gGovUIOfferDays[playerid],offerDayWord,",UI)
        self.assertIn("gGovUIMaturityDays[playerid],maturityDayWord);",UI)

    def test_signing_and_approval_are_not_duplicate_menu_actions(self):
        # BOND-UI-09: signing a draft is the only approval entry point.
        issue=UI.split("if(dialogid==GOVUI_ISSUE)",1)[1].split(
            "if(dialogid==GOVUI_ACTION)",1)[0]
        actions=UI.split("stock GovBond_UI_Actions(playerid)",1)[1].split(
            "stock GovBond_UI_ConfirmAction(playerid,action)",1)[0]
        menu_handler=UI.split("if(dialogid==GOVUI_ACTION)",1)[1].split(
            "if(dialogid==GOVUI_APPROVAL_REASON)",1)[0]
        self.assertIn('body,"Подписать","Отклонить"',UI)
        self.assertIn("gGovUIAction[playerid]=2;",issue)
        self.assertIn("return GovBond_UI_ExecuteAction(playerid);",issue)
        self.assertNotIn("Утвердить выпуск (заместитель/министр)",actions)
        self.assertIn("Открыть размещение (заместитель/министр)",actions)
        self.assertIn("Отменить непроданный выпуск (заместитель/министр)",actions)
        self.assertIn("Вернуться к реестру",actions)
        self.assertIn("if(listitem==0)return GovBond_UI_ConfirmAction(playerid,3);",menu_handler)
        self.assertIn("if(listitem==1)return GovBond_UI_ConfirmAction(playerid,4);",menu_handler)
        self.assertNotIn("listitem+2",menu_handler)
        self.assertIn("CALL gov_bond_official_approve",UI)
        self.assertIn("CALL gov_bond_official_open",UI)

    def test_draft_confirmation_highlights_all_values(self):
        # BOND-UI-08: every field value is yellow, but labels remain light.
        block=UI.split("stock GovBond_UI_ConfirmDraft(playerid)",1)[1].split(
            "// A rejection is final",1)[0]
        for label,template in (
            ("Название", "%s"),
            ("Цель", "%s"),
            ("Тип", "%s"),
            ("Номинал", "$%d"),
            ("Количество", "%d"),
            ("Общий номинал", "$%d"),
            ("Доходность", "%d.%02d%%"),
            ("Размещение", "%d %s"),
            ("Погашение через", "%d %s после размещения"),
        ):
            with self.subTest(label=label):
                self.assertIn(f"{{E8EDF3}}{label}: {{FFD15C}}{template}",block)
        self.assertIn("Сохраняется код выпуска {FFD15C}%s{E8EDF3}.",block)
        self.assertIn("{E8EDF3}Отправка заявки не открывает продажу",block)
        self.assertIn("gGovUIFace[playerid]*gGovUIQuantity[playerid]",block)
        self.assertIn("new body[900]",block)
        self.assertIn("GOVUI_CONFIRM,DIALOG_STYLE_MSGBOX",block)

    def test_fund_lessons_explain_state_treasury_not_minister_wallet(self):
        # User acceptance test 5.1: the fund reserves treasury funds, not player cash.
        intro=EDU.split("case 3: format(body,sizeof(body),",1)[1].split("case 4:",1)[0]
        lesson=EDU.split("case 9: format(body,sizeof(body),",1)[1].split("case 10:",1)[0]
        faq=EDU.split("case 11: format(body,sizeof(body),",1)[1].split("gBondEduTopic[playerid]=topic;",1)[0]
        self.assertIn("из казны, а не из своего кармана",intro)
        self.assertIn("не из личного кошелька министра",lesson)
        self.assertIn("Личные деньги игрока не списываются.",lesson)
        self.assertIn("Общий баланс казны не меняется",lesson)
        self.assertIn("страховой резерв растёт на $1000",lesson)
        self.assertIn("свободных денег становится на $1000 меньше",lesson)
        self.assertIn("ПОЧЕМУ НЕ СПИСАЛИСЬ ДЕНЬГИ У МИНИСТРА?",faq)
        self.assertIn("return BondEducation_Open(playerid,1);",UI)
        self.assertIn("BOND_EDU_TOPIC_COUNT (12)",EDU)
        sql=(ROOT/"db/migrations/029_minfin_fund_capitalization.sql").read_text("utf-8")
        self.assertIn("bond_fund_reserved=bond_fund_reserved+p_amount",sql)
        self.assertNotIn("SET balance=balance-p_amount",sql)
        self.assertNotIn("UPDATE accounts",sql)

    def test_fund_topup_dialog_highlights_values_and_status(self):
        # BOND-UI-07: only this finance form uses semantic colors.
        block=UI.split("if (kind==6)",1)[1].split("if (kind==1)",1)[0]
        self.assertEqual(block.count("{FFD15C}$%d"),6)
        self.assertIn("{E8EDF3}Казна: {FFD15C}$%d",block)
        self.assertIn("{E8EDF3}Обязательный остаток: {FFD15C}$%d",block)
        self.assertIn("{E8EDF3}Страховой резерв: {FFD15C}$%d",block)
        self.assertIn("{E8EDF3}Выдано гарантий: {FFD15C}$%d",block)
        self.assertIn("{E8EDF3}Свободно для перевода: {FFD15C}$%d",block)
        self.assertIn("{E8EDF3}Введите сумму от {FFD15C}$1{E8EDF3} до {FFD15C}$%d{E8EDF3}.",block)
        self.assertIn("enabled?\"{78D8AA}Включён\":\"{E48A8A}Выключен\"",block)
        self.assertIn("{E8EDF3}Статус фонда: %s{E8EDF3}.",block)
        self.assertIn("cash,minReserve,reserved,committed,freeBudget,maximum,",block)
        self.assertIn('GOVUI_FUND_AMOUNT,DIALOG_STYLE_INPUT',block)

    def test_distinct_dialog_ids(self):
        import re
        ids=re.findall(r"#define\s+GOVUI_(?:MAIN|TYPE|INPUT|CONFIRM|LIST|ISSUE|ACTION|REPORT|APPROVAL_REASON|FUND_AMOUNT|FUND_CONFIRM)\s+\((\d+)\)",UI)
        self.assertEqual(set(map(int,ids)),set(range(6800,6811)))


if __name__=="__main__":
    unittest.main()
