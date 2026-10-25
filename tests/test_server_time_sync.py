"""Default environment follows the game host clock, not a hard-coded 03:17."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
ADMIN = (ROOT / "gamemodes/includes/admin_system.inc").read_text("utf-8")
TOOLS = (ROOT / "gamemodes/includes/admin_tools2.inc").read_text("utf-8")
HELP = (ROOT / "gamemodes/includes/admin_help.inc").read_text("utf-8")


class ServerClockTests(unittest.TestCase):
    def test_default_server_weather_and_os_clock(self):
        init = ADMIN.split("stock Admin_Init()", 1)[1].split("\n}\n", 1)[0]
        self.assertIn("gAdminWeather = 9;", init)
        self.assertIn("gAdminClockAuto = true;", init)
        self.assertIn("gettime(gAdminClockHour, gAdminClockMinute, second);", init)
        self.assertNotIn("gAdminClockHour = 3;", init)
        self.assertNotIn("gAdminClockMinute = 17;", init)

    def test_periodic_time_sync_and_cleanup(self):
        self.assertIn("#define ADMIN_CLOCK_SYNC_MS (15000)", ADMIN)
        self.assertIn('SetTimer("Admin_ClockTick", ADMIN_CLOCK_SYNC_MS, true)', ADMIN)
        self.assertIn("public Admin_ClockTick()", ADMIN)
        self.assertIn("Admin_ClockRefresh();", ADMIN)
        self.assertIn("if (Admin_Ready(i)) SetPlayerTime(i, gAdminClockHour, gAdminClockMinute)", ADMIN)
        self.assertIn("KillTimer(gAdminClockTimer);", ADMIN)
        self.assertIn("Admin_ClockShutdown();", GAME)
        self.assertIn("if (gAdminClockAuto)", ADMIN)

    def test_class_selection_and_spawn_never_force_0317(self):
        class_request = GAME.split("public OnPlayerRequestClass(playerid, classid)", 1)[1].split(
            "public OnPlayerRequestSpawn", 1)[0]
        spawn = GAME.split("public OnPlayerSpawn(playerid)", 1)[1].split(
            "public OnPlayerDeath", 1)[0]
        self.assertIn("Admin_ApplyEvent(playerid);", class_request)
        self.assertIn("Admin_ApplyEvent(playerid);", spawn)
        for part in (class_request, spawn):
            self.assertNotIn("SetPlayerTime(playerid, 3, 17)", part)
            self.assertNotIn("SetPlayerWeather(playerid, 9)", part)

    def test_admin_overrides_are_preserved_and_reversible(self):
        self.assertIn("gAdminClockAuto = false;", TOOLS)
        self.assertIn('!strcmp(params, "auto", true)', TOOLS)
        self.assertIn('Admin_LogPlayer(playerid, "SETTIME", "auto")', TOOLS)
        self.assertLess(TOOLS.index('Admin_LogPlayer(playerid, "SETTIME", "auto")'),
                        TOOLS.index("gAdminClockAuto = true;"))
        self.assertIn("Admin_ClockTick();", TOOLS)
        self.assertIn("gAdminClockAuto = false;", ADMIN)  # fog / night
        event_clear = ADMIN.split('else if (!strcmp(arg1, "clear", true))', 1)[1].split(
            "else return AdminHelp_Usage(playerid, \"event\");", 1)[0]
        self.assertIn("gAdminClockAuto = true;", event_clear)
        self.assertIn("Admin_ClockRefresh();", event_clear)
        self.assertIn("gAdminWeather = 9;", event_clear)
        self.assertIn("/settime auto", HELP)
        self.assertIn("SetPlayerWeather(playerid, gAdminWeather);", ADMIN)

    def test_cp1251_build_encoding(self):
        for source in (GAME, ADMIN, TOOLS, HELP):
            self.assertEqual(source, source.encode("cp1251").decode("cp1251"))


if __name__ == "__main__":
    unittest.main()
