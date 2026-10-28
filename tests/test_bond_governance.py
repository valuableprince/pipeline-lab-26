"""GOV-05 official console, automatic issue codes and role separation contracts."""
from pathlib import Path
import unittest
from build_contracts import assert_staged

ROOT=Path(__file__).resolve().parents[1]
SQL=(ROOT/"db/migrations/023_bond_governance.sql").read_text("utf-8")
UI=(ROOT/"gamemodes/includes/bond_governance_ui.inc").read_text("utf-8")
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
            "INSERT INTO schema_migrations(version) VALUES (23);"):
            with self.subTest(phrase=phrase):self.assertIn(phrase,SQL)
        create=SQL.split("CREATE PROCEDURE gov_bond_issue_create_auto(",1)[1].split("END$$",1)[0]
        for phrase in ("START TRANSACTION;","ROLLBACK;","COMMIT;",
          "f.term_end>UNIX_TIMESTAMP() FOR UPDATE","v_rank NOT IN (2,9,10)",
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

    def test_ministry_approval_and_no_second_signature_for_leaders(self):
        block=SQL.split("CREATE PROCEDURE gov_bond_official_approve(",1)[1].split("END$$",1)[0]
        self.assertIn("faction_members",block)
        self.assertIn("m.faction_id=5",block)
        self.assertIn("v_rank NOT IN (9,10)",block)
        self.assertIn("CALL gov_bond_issue_approve(p_event_key,p_issue_code,p_actor)",block)
        self.assertIn("INSERT INTO factions(faction_id,is_enabled,term_start,term_end)",SQL)
        self.assertNotIn("government_bond_officials",SQL)
        self.assertNotIn("gov_bond_role_set",SQL)
        core=(ROOT/"db/migrations/020_government_bond_issues.sql").read_text("utf-8")
        self.assertIn("v_creator=p_actor_id AND NOT EXISTS",core)
        self.assertIn("m.rank_id IN (9,10)",core)

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
          "bond_crisis_state",
          "bond_fund_reserved",
          "bond_payout_reserved",
          "government_bond_claims",
          "GOVUI_LIST,DIALOG_STYLE_TABLIST_HEADERS",
          "GOVUI_CONFIRM,DIALOG_STYLE_MSGBOX"):
            with self.subTest(phrase=phrase):self.assertIn(phrase,UI)
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

    def test_distinct_dialog_ids(self):
        import re
        ids=re.findall(r"#define\s+GOVUI_(?:MAIN|TYPE|INPUT|CONFIRM|LIST|ISSUE|ACTION|REPORT)\s+\((\d+)\)",UI)
        self.assertEqual(set(map(int,ids)),set(range(6800,6808)))


if __name__=="__main__":
    unittest.main()
