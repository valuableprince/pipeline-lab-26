"""Contracts for the character menu and authoritative MariaDB identity."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
CMDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
MENU = (ROOT / "gamemodes/includes/player_menu.inc").read_text(encoding="utf-8")
DB = (ROOT / "gamemodes/includes/db_primary.inc").read_text(encoding="utf-8")
PAYDAY = (ROOT / "gamemodes/includes/payday.inc").read_text(encoding="utf-8")


class PlayerMenuContracts(unittest.TestCase):
    def test_three_commands_same_menu_no_level_command(self):
        for alias in ("mn", "mm", "menu"):
            self.assertIn(f"cmd:{alias}(playerid, params[])\n{{\n    return PlayerMenu_Open(playerid);", CMDS)
        self.assertNotIn("cmd:level(", CMDS)
        self.assertIn("Статистика персонажа", MENU)
        self.assertIn('"Статистика персонажа", body, "Назад", "Закрыть"', MENU)
        self.assertNotIn("TARKENSO | Статистика персонажа", MENU)

    def test_player_stats_use_real_sql_account_identity(self):
        self.assertIn("gDBPrimaryAccountId[playerid]", MENU)
        self.assertIn("DATE(created_at) AS registered_date", DB)
        self.assertIn("gDBPrimaryRegistrationDate[playerid]", DB)
        self.assertIn("gDBPrimaryRegistrationDate[playerid]", MENU)
        self.assertIn("LAST_INSERT_ID()", DB)
        self.assertIn("gCharacterEXP[playerid]", MENU)
        self.assertIn("gCharacterLevel[playerid]", MENU)
        self.assertIn("level * PAYDAY_EXP_STEP", MENU)
        self.assertNotIn("getdate(", MENU)
        self.assertNotIn("gettime(", MENU)

    def test_dialog_lifecycle_and_auth_guard(self):
        self.assertIn('#include "player_menu.inc"', GAME)
        self.assertIn("PlayerMenu_OnDialog(playerid, dialogid, response, listitem)", GAME)
        self.assertIn("gAuthState[playerid] != AUTH_READY", MENU)
        self.assertIn("PLAYER_MENU_MAIN", MENU)
        self.assertIn("PLAYER_MENU_STATS", MENU)
        self.assertIn("DIALOG_STYLE_LIST", MENU)
        self.assertIn("DIALOG_STYLE_MSGBOX", MENU)
        self.assertIn("if (response) return PlayerMenu_Open(playerid);", MENU)
        self.assertIn('"%d/%d"', MENU)
        self.assertIn("PayDay_Grant(playerid, pending)", PAYDAY)
        self.assertIn("gPayDayPendingHour[playerid]", PAYDAY)


if __name__ == "__main__":
    unittest.main()
