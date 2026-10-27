"""SERVER-GUARD-01: статическая регрессия интеграции диагностики и антифлуда."""

from pathlib import Path
from build_contracts import assert_staged
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
GUARD = (ROOT / "gamemodes/includes/server_guard.inc").read_text(encoding="utf-8")
DOC = (ROOT / "docs/SERVER_GUARD.md").read_text(encoding="utf-8")


class ServerGuardTests(unittest.TestCase):
    def test_integrated_and_staged_in_both_builds(self):
        self.assertIn('#include "server_guard.inc"', GAME)
        self.assertLess(GAME.index('#include "admin_system.inc"'),
                        GAME.index('#include "server_guard.inc"'))
        self.assertLess(GAME.index('#include "server_guard.inc"'),
                        GAME.index('#include "commands.inc"'))
        assert_staged(self, "server_guard.inc")

    def test_hook_lifecycle(self):
        for call in ("Guard_Init();", "Guard_Shutdown();", "Guard_OnConnect(playerid);",
                     "Guard_OnDisconnect(playerid);"):
            self.assertIn(call, GAME)
        self.assertIn("if (!Guard_AllowChat(playerid)) return 0;", GAME)
        self.assertIn("if (!Guard_AllowCommand(playerid)) return 0;", COMMANDS)
        self.assertIn("Guard_CommandStart(playerid);", COMMANDS)
        self.assertIn("Guard_CommandFinish(playerid);", COMMANDS)
        self.assertLess(GAME.index("if (!Guard_AllowChat(playerid)) return 0;"),
                        GAME.index("if (gAdminMuteUntil[playerid] > gettime())"))
        self.assertLess(COMMANDS.index("if (!Guard_AllowCommand(playerid)) return 0;"),
                        COMMANDS.index("if (gAuthState[playerid] != AUTH_READY)"))

    def test_observation_and_soft_limits(self):
        for constant in ("GUARD_CHAT_LIMIT               (6)",
                         "GUARD_COMMAND_LIMIT            (15)",
                         "GUARD_HEARTBEAT_WARNING_MS     (2500)",
                         "GUARD_CONNECTION_ALERT         (30)"):
            self.assertIn(constant, GUARD)
        for event in ('"CHAT_FLOOD"', '"COMMAND_FLOOD"',
                      '"CONNECTION_SPIKE"', '"SLOW_COMMAND"', '"HEARTBEAT_DELAY"'):
            self.assertIn(event, GUARD)
        self.assertIn("return false;", GUARD)
        self.assertNotIn("Kick(playerid)", GUARD)
        self.assertNotIn("Ban(playerid)", GUARD)
        self.assertIn("if (IsPlayerNPC(playerid)) return 1;", GUARD)
        self.assertNotIn("GetPlayerIp", GUARD)
        self.assertNotIn("text[]", GUARD)

    def test_permissions_and_documentation(self):
        self.assertIn("cmd:guardstatus(playerid, params[])", COMMANDS)
        self.assertIn("Admin_Allowed(playerid, PERM_REPORT)", GUARD)
        self.assertIn("SERVER-GUARD-01", DOC)
        self.assertIn("scriptfiles/server_guard.log", DOC)


if __name__ == "__main__":
    unittest.main()
