"""RP-CHAT-01: source-level regression checks for local roleplay chat."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
RP = (ROOT / "gamemodes/includes/rp_chat.inc").read_text(encoding="utf-8")


class RolePlayChatTests(unittest.TestCase):
    def test_nine_commands_registered_exactly_once(self):
        for name in ("me", "do", "try", "todo", "ame", "b", "s", "w", "low"):
            self.assertEqual(
                len(re.findall(rf"^cmd:{name}\(playerid, params\[\]\)", COMMANDS, re.MULTILINE)),
                1, name,
            )
        self.assertIn('!strcmp(category, "rp", true)', COMMANDS)
        self.assertIn("/help rp", COMMANDS)

    def test_respects_chat_mute_auth_and_flood(self):
        self.assertIn("gAuthState[playerid] != AUTH_READY", RP)
        self.assertIn("!gAccountSpawned[playerid]", RP)
        self.assertIn("gAdminMuteUntil[playerid] > gettime()", RP)
        self.assertIn("Guard_AllowChat(playerid)", RP)
        self.assertIn("Guard_AllowCommand(playerid)", COMMANDS)

    def test_local_delivery_filters_world_interior_and_radius(self):
        self.assertIn("GetPlayerVirtualWorld(i) != world", RP)
        self.assertIn("GetPlayerInterior(i) != interior", RP)
        self.assertIn("maxDistSquared", RP)
        self.assertIn("IsPlayerConnected(i)", RP)
        self.assertIn("SendClientMessage(i, color, message)", RP)
        self.assertNotIn("SendClientMessageToAll", RP)

    def test_sanitization_and_strict_limits(self):
        self.assertIn("RP_TEXT_MAX", RP)
        self.assertIn("length - start > RP_TEXT_MAX", RP)
        self.assertIn("if (ch == '{') ch = '(';", RP)
        self.assertIn("else if (ch == '}') ch = ')';", RP)
        self.assertIn('strmid(tail, input, split + 1, originalLength', RP)
        self.assertIn("random(2)", RP)
        RP.encode("cp1251", errors="strict")

    def test_ame_label_does_not_survive_disconnect(self):
        self.assertIn("RP_Init();", GAME)
        self.assertEqual(GAME.count("RP_Reset(playerid);"), 2)
        self.assertIn("RP_ActionExpire", RP)
        self.assertIn("gRPActionSerial[playerid] == serial", RP)
        self.assertIn("Attach3DTextLabelToPlayer", RP)
        self.assertIn("Delete3DTextLabel", RP)
        self.assertIn('#include "rp_chat.inc"', GAME)


if __name__ == "__main__":
    unittest.main()
