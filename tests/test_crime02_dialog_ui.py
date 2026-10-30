"""CRIME-02.1 dialog UX: one command, bounded, session-safe interactions."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / "gamemodes/includes/crime_ui.inc").read_text(encoding="utf-8")
CORE = (ROOT / "gamemodes/includes/crime_contracts.inc").read_text(encoding="utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
LEADER = (ROOT / "gamemodes/includes/leader_ui.inc").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
HELP = (ROOT / "gamemodes/includes/player_command_help.inc").read_text(encoding="utf-8")


class CrimeDialogTests(unittest.TestCase):
    def test_only_one_crime_command(self):
        self.assertIn("cmd:crime(playerid, params[])", COMMANDS)
        self.assertIn("return CrimeUI_Main(playerid);", COMMANDS)
        self.assertNotIn("return Crime_Command(", COMMANDS)
        self.assertIn('{"crime", "/crime",', HELP)
        self.assertNotIn("/crime [действие]", HELP)
        self.assertNotIn("stock Crime_Command(", CORE)
        self.assertIn('if (Leader_IsCriminal(org)) strcat(menu,', LEADER)
        self.assertIn('case 5: return CrimeUI_Main(playerid);', LEADER)

    def test_dialog_id_allocation_and_entrypoint(self):
        ids = re.findall(r"#define CRIME_DLG_[A-Z_]+\s+\((\d+)\)", UI)
        self.assertEqual([int(x) for x in ids], list(range(7950, 7968)))
        self.assertIn('#include "crime_ui.inc"', GAME)
        self.assertIn('CrimeUI_OnDialog(playerid, dialogid, response, listitem)', GAME)
        self.assertEqual(GAME.count("CrimeUI_Reset(playerid);"), 2)
        self.assertIn("DIALOG_STYLE_TABLIST_HEADERS", UI)
        self.assertIn("DIALOG_STYLE_MSGBOX", UI)

    def test_all_actions_are_in_dialog_handler(self):
        for name in ("CrimeUI_Main", "CrimeUI_Contacts", "CrimeUI_ContactDetail", "CrimeUI_Offers", "CrimeUI_OfferDetail",
                     "CrimeUI_Active", "CrimeUI_Route", "CrimeUI_Cancel",
                     "CrimeUI_History", "CrimeUI_Help", "CrimeUI_OnDialog"):
            self.assertIn("stock " + name + "(", UI)
        self.assertIn("case 0: return CrimeUI_Contacts(playerid);", UI)
        self.assertIn("case 1: return CrimeUI_Active(playerid);", UI)
        self.assertIn("case 2: return CrimeUI_Route(playerid);", UI)
        self.assertIn("case 3: return CrimeUI_History(playerid);", UI)
        self.assertIn("case 4: return CrimeUI_History(playerid, true);", UI)
        self.assertIn("case 5: return CrimeUI_Cancel(playerid);", UI)
        self.assertIn("case 6: return CrimeUI_Help(playerid);", UI)
        self.assertIn("Crime_Start(playerid, gCrimeUISelectedTemplate[playerid]);", UI)
        self.assertIn("Crime_Advance(playerid, phase == 2);", UI)
        self.assertIn("Crime_Abort(playerid);", UI)

    def test_security_recheck_and_no_accidental_cancel(self):
        handler = UI.split("stock CrimeUI_OnDialog(", 1)[1]
        for token in (
            "gCrimeUIDialog[playerid] != dialogid",
            "gCrimeUIAuthToken[playerid] != gAuthToken[playerid]",
            "gCrimeUIAccountId[playerid] != gDBPrimaryAccountId[playerid]",
            "!Crime_Available(playerid)",
            "gCrimeUIDialog[playerid] = 0;",
            "if (!response)",
            "if (listitem < 0 || listitem > 6)",
            "if (listitem < 0 || listitem >= gCrimeUIOfferCount[playerid])",
        ):
            self.assertIn(token, handler)
        self.assertLess(handler.index("if (!response)"),
                        handler.index("Crime_Start(playerid, gCrimeUISelectedTemplate[playerid]);"))
        self.assertLess(handler.index("if (!response)"),
                        handler.index("Crime_Abort(playerid);"))
        for token in ("Crime_LoadActive(", "Crime_Expire(", "IsPlayerInRangeOfPoint",
                      "faction != Leader_PlayerOrg(playerid)",
                      "AND state=1 AND expires_at>UNIX_TIMESTAMP()"):
            self.assertIn(token, CORE)

    def test_no_commands_required_for_delivery(self):
        for deprecated in ('"/crime pickup"', '"/crime deliver"',
                           '"/crime offer"', '"/crime route"'):
            self.assertNotIn(deprecated, UI)
            self.assertNotIn(deprecated, CORE)
        self.assertIn("case CRIME_DLG_ROUTE:", UI)
        self.assertIn("case CRIME_DLG_CANCEL:", UI)
        self.assertIn('"{FFD15C}Теневой рынок | Обучение"', UI)


if __name__ == "__main__":
    unittest.main()
