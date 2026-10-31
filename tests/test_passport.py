"""PASSPORT-01: safety and integration contracts for personal documents."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
PASSPORT = (ROOT / "gamemodes/includes/passport.inc").read_text(encoding="utf-8")
CMDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
MENU = (ROOT / "gamemodes/includes/player_menu.inc").read_text(encoding="utf-8")
HELP = (ROOT / "gamemodes/includes/player_command_help.inc").read_text(encoding="utf-8")
AUTH = (ROOT / "gamemodes/includes/auth_dialogs.inc").read_text(encoding="utf-8")
MIG = (ROOT / "db/migrations/047_passports.sql").read_text(encoding="utf-8")
DOC = (ROOT / "docs/PASSPORT_SYSTEM.md").read_text(encoding="utf-8")


class PassportTests(unittest.TestCase):
    def test_unique_persistent_serial_without_revealing_account_id(self):
        for phrase in (
            "passport_id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY",
            "UNIQUE KEY uq_player_passports_account (account_id)",
            "REFERENCES accounts(account_id) ON DELETE RESTRICT",
            "SELECT account_id, DATE(created_at) FROM accounts ORDER BY account_id",
            "INSERT INTO schema_migrations (version) VALUES (47)",
            "AUTO_INCREMENT=500001",
        ):
            self.assertIn(phrase, MIG)
        self.assertIn("TK-%06d", PASSPORT)
        self.assertIn("gCharacterLevel[playerid]", PASSPORT)
        self.assertIn("ON DUPLICATE KEY UPDATE account_id=account_id", PASSPORT)
        self.assertIn("birth_date IS NULL", PASSPORT)
        self.assertIn("changed != 1", PASSPORT)

    def test_integrated_with_auth_and_fail_closed_migration(self):
        for phrase in ('#include "passport.inc"', "if (!Passport_Init())",
                       "Passport_OnReady(playerid);", "case DIALOG_BIRTH:",
                       "AUTH_REGISTER_BIRTH",
                       "Passport_OnDialog(playerid, dialogid, response, inputtext)"):
            self.assertIn(phrase, GAME)
        self.assertIn("FROM schema_migrations WHERE version=47", PASSPORT)
        self.assertEqual(GAME.count("Passport_Reset(playerid);"), 2)
        self.assertIn("AuthDialog_BirthDate(playerid, error);", GAME)
        self.assertIn("gPassportPendingBirth[playerid]", GAME)

    def test_dob_is_calendar_checked_and_write_once(self):
        for phrase in ("strlen(input) != 10", "input[2] != '.'",
                       "input[5] != '.'", "case 4, 6, 9, 11",
                       "year % 400 == 0", "getdate(currentYear, currentMonth, currentDay)",
                       "if (day < 1 || day > days)", "if (age < 18 || age > 100)",
                       "gPassportBirthDialog[playerid]",
                       "UPDATE player_passports SET birth_date='%e'",
                       "WHERE passport_id=%d AND account_id=%d AND birth_date IS NULL"):
            self.assertIn(phrase, PASSPORT)
        self.assertIn("DIALOG_STYLE_INPUT", PASSPORT)
        self.assertIn("Passport_BirthPrompt(playerid)", PASSPORT)
        self.assertIn('gPassportBirth[playerid][0] ? "" : "Указать дату"', PASSPORT)
        self.assertIn('if (!response && gAuthState[playerid] == AUTH_READY &&', PASSPORT)
        self.assertIn('"Не указана"', PASSPORT)
        open_body = PASSPORT.split("stock Passport_Open(playerid)", 1)[1].split(
            "stock Passport_Show(playerid, target)", 1
        )[0]
        self.assertNotIn("return Passport_BirthPrompt(playerid)", open_body)
        self.assertIn("Passport_Build(playerid, body, sizeof(body));", open_body)

    def test_sql_gate_uses_mysql_format_not_string_format(self):
        self.assertNotIn("format(query, sizeof(query),", PASSPORT)
        self.assertNotIn("format(q, sizeof(q),", PASSPORT)
        self.assertIn("mysql_format(gDBPrimary, query,", PASSPORT)
        self.assertIn("mysql_format(gDBPrimary, q,", PASSPORT)
        self.assertEqual(PASSPORT.count("birth_date='%e'"), 2)

    def test_legacy_accounts_can_show_passport_without_birthdate(self):
        show = PASSPORT.split("stock Passport_Show(playerid, target)", 1)[1].split(
            "stock Passport_OnDialog(playerid", 1
        )[0]
        self.assertNotIn("!gPassportBirth[playerid][0]", show)
        self.assertIn("!gPassportID[playerid] && !Passport_Load(playerid)", show)
        self.assertIn("Passport_Build(playerid, body, sizeof(body));", show)
        self.assertIn("birth_date DATE DEFAULT NULL", MIG)

    def test_showpass_restricted_to_nearby_authenticated_players(self):
        for phrase in ("target == playerid", "gAuthState[target] != AUTH_READY",
                       "!gAccountSpawned[target]",
                       "GetPlayerVirtualWorld(playerid) != GetPlayerVirtualWorld(target)",
                       "GetPlayerInterior(playerid) != GetPlayerInterior(target)",
                       "IsPlayerInRangeOfPoint(target, PASSPORT_SHOW_RADIUS",
                       "gettime() < gPassportNextShow[playerid]",
                       "PASSPORT_SHOW_COOLDOWN  (5)"):
            self.assertIn(phrase, PASSPORT)

    def test_menu_help_ui_and_cp1251(self):
        for command in ("pass", "showpass"):
            self.assertIn(f"cmd:{command}(playerid, params[])", CMDS)
            self.assertIn(f'{{"{command}", "/{command}', HELP)
        self.assertIn("4. Документы", MENU)
        self.assertIn("PLAYER_MENU_DOCUMENTS", MENU)
        self.assertIn("return Passport_Open(playerid);", MENU)
        for label in ("Категория резидента", "Дата рождения", "Бессрочно",
                      "Штат Сан-Андреас", "DIALOG_STYLE_MSGBOX"):
            self.assertIn(label, PASSPORT)
        for source in (GAME, PASSPORT, CMDS, MENU, HELP, AUTH):
            source.encode("cp1251", errors="strict")
        self.assertIn("отдельный SQL AUTO_INCREMENT", DOC)


if __name__ == "__main__":
    unittest.main()
