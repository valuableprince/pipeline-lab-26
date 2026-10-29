"""Onboarding contracts: menu, shared finance textbook and safe command catalog."""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
MENU=(ROOT/"gamemodes/includes/player_menu.inc").read_text("utf-8")
BOOK=(ROOT/"gamemodes/includes/bond_education.inc").read_text("utf-8")
COMMANDS=(ROOT/"gamemodes/includes/player_command_help.inc").read_text("utf-8")
GOV=(ROOT/"gamemodes/includes/bond_governance_ui.inc").read_text("utf-8")
GAME=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
ROUTER=(ROOT/"gamemodes/includes/commands.inc").read_text("utf-8")

class PlayerOnboardingTests(unittest.TestCase):
    def test_character_menu_has_both_help_sections(self):
        self.assertIn("1. Статистика персонажа",MENU)
        self.assertIn("2. Помощь по командам",MENU)
        self.assertIn("3. Помощь по игре",MENU)
        self.assertIn("if(listitem==1)return CommandHelp_Categories(playerid);",MENU)
        self.assertIn("if(listitem==2)return PlayerMenu_GameHelp(playerid);",MENU)
        for alias in ("mn","mm","menu"):
            self.assertIn("cmd:"+alias+"(playerid, params[])",ROUTER)
        self.assertIn("BondEducation_Open(playerid,0)",MENU)
        self.assertIn("return BondEducation_Open(playerid,1);",GOV)

    def test_one_textbook_serves_player_and_ministry(self):
        self.assertEqual(BOOK.count('stock BondEducation_Open('),1)
        self.assertIn('gBondEduSource[playerid]=fromMinfin',BOOK)
        self.assertIn('return GovBond_UI_Main(playerid);',BOOK)
        self.assertIn('return PlayerMenu_GameHelp(playerid);',BOOK)
        self.assertIn('topic>=BOND_EDU_TOPIC_COUNT',BOOK)
        self.assertIn('listitem>=BOND_EDU_TOPIC_COUNT',BOOK)
        self.assertIn('if(!response)return BondEducation_Open(',BOOK)
        for concept in ("НОМИНАЛ","КУПОН","PAYDAY","ПОГАШЕНИЕ","ПРОСРОЧКА",
                        "СТРАХОВОЙ ФОНД","ПРИНЯТЫЕ ГАРАНТИИ",
                        "20%","Пополнить страховой фонд","Подписать"):
            with self.subTest(concept=concept):
                self.assertIn(concept,BOOK)
        self.assertIn("{FFFFFF}",BOOK)
        self.assertIn("{FFD15C}",BOOK)
        self.assertNotIn("CALL gov_",BOOK)
        self.assertEqual(len(re.findall(r"case \d+: format\(body,sizeof\(body\)",BOOK)),12)

    def test_command_catalog_is_filtered_and_dialogs_are_routed(self):
        self.assertIn("DIALOG_STYLE_TABLIST_HEADERS",COMMANDS)
        self.assertIn("chUsage[90]",COMMANDS)
        self.assertIn("chPurpose[116]",COMMANDS)
        self.assertIn("chExample[110]",COMMANDS)
        self.assertIn("Leader_PlayerOrg(playerid)==FACTION_FINANCE",COMMANDS)
        self.assertIn("gAdminLevel[playerid]>0",COMMANDS)
        self.assertIn("CommandHelp_Access(playerid,i)",COMMANDS)
        self.assertIn("CommandHelp_Access(playerid,index)",COMMANDS)
        self.assertIn('{"bondgov"',COMMANDS)
        self.assertIn('{"ahelp"',COMMANDS)
        self.assertIn('{"r"',COMMANDS)
        self.assertIn("gCmdHelpRows[playerid][listitem]",COMMANDS)
        self.assertIn("gCmdHelpPage[playerid]++",COMMANDS)
        self.assertIn("gCmdHelpPage[playerid]--",COMMANDS)
        self.assertIn("if(!response)return PlayerMenu_Open(playerid)",COMMANDS)
        for inc in ("bond_education.inc","player_command_help.inc"):
            self.assertIn('#include "'+inc+'"',GAME)
        self.assertIn("CommandHelp_OnDialog(playerid, dialogid, response, listitem)",GAME)
        self.assertIn("BondEducation_OnDialog(playerid, dialogid, response, listitem)",GAME)

    def test_no_reused_dialog_identifiers(self):
        tokens={"PLAYER_MENU_MAIN":6430,"PLAYER_MENU_STATS":6431,
                "PLAYER_MENU_GAME_HELP":6432,"PLAYER_MENU_GAME_TOPIC":6433,
                "BOND_EDU_CATALOG":7900,"BOND_EDU_DETAIL":7901,
                "CMD_HELP_CATEGORIES":7902,"CMD_HELP_LIST":7903,
                "CMD_HELP_DETAIL":7904}
        for name,number in tokens.items():
            source=BOOK if name.startswith("BOND_EDU") else COMMANDS if name.startswith("CMD_HELP") else MENU
            self.assertRegex(source,r"#define\s+"+name+r"\s+\("+str(number)+r"\)")
        self.assertEqual(len(set(tokens.values())),len(tokens))

if __name__=="__main__":
    unittest.main()
