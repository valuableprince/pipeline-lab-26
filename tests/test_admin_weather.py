"""Контракт команды /weather и единой погоды во всех сессиях игроков."""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
ADMIN = (ROOT / "gamemodes/includes/admin_system.inc").read_text("utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
MENU = (ROOT / "gamemodes/includes/player_menu.inc").read_text("utf-8")
AUTH = (ROOT / "gamemodes/includes/auth_dialogs.inc").read_text("utf-8")


class WeatherAndBrandingTests(unittest.TestCase):
    def test_weather_is_admin_only_and_audited_before_change(self):
        self.assertIn("cmd:weather(playerid, params[])", COMMANDS)
        handler = ADMIN.split('if (!strcmp(command, "/weather", true))', 1)[1].split(
            'if (!strcmp(command, "/event", true))', 1)[0]
        for expression in (
            "Admin_Require(playerid, PERM_MODERATE)",
            "Admin_UnsignedNumber(arg1)",
            "Admin_Next(input, offset, arg2, sizeof(arg2))",
            "weatherid > 45",
            'Admin_LogPlayer(playerid, "WEATHER", detail)',
            "gAdminWeather = weatherid;",
            "if (Admin_Ready(i)) Admin_ApplyEvent(i);",
        ):
            self.assertIn(expression, handler)
        self.assertLess(handler.index('Admin_LogPlayer(playerid, "WEATHER", detail)'),
                        handler.index("gAdminWeather = weatherid;"))

    def test_weather_applies_on_respawn_and_event_is_compatible(self):
        self.assertIn("gAdminWeather = 9;", ADMIN)
        self.assertIn("SetPlayerWeather(playerid, gAdminWeather);", ADMIN)
        self.assertIn("Admin_ApplyEvent(playerid);", GAME)
        event_handler = ADMIN.split('if (!strcmp(command, "/event", true))', 1)[1]
        self.assertIn("gAdminWeather = 20;", event_handler)
        self.assertIn("gAdminWeather = 9;", event_handler)

    def test_character_statistics_are_kept_authoritative(self):
        self.assertIn('"Статистика персонажа", body, "Назад", "Закрыть"', MENU)
        self.assertNotIn("TARKENSO | Статистика персонажа", MENU)
        for token in ("gCharacterEXP[playerid]", "gCharacterLevel[playerid]",
                      "gDBPrimaryRegistrationDate[playerid]", "gDBPrimaryAccountId[playerid]"):
            self.assertIn(token, MENU)

    def test_branding_and_technical_compatibility(self):
        self.assertIn('SendRconCommand("hostname Tarkenso RPG");', GAME)
        self.assertIn("Tarkenso RPG", AUTH)
        self.assertNotIn("OPENMP SERVER", AUTH)
        self.assertIn('"SIGNAL0317|5"', GAME)
        self.assertIn('#if defined TARKENSO_DB_PRIMARY', GAME)
        for source in (GAME, COMMANDS, MENU, ADMIN, AUTH):
            self.assertEqual(source, source.encode("cp1251").decode("cp1251"))


if __name__ == "__main__":
    unittest.main()
