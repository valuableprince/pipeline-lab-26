"""CHAT-LOCAL-01: normal chat shares RP proximity and keeps moderation guards."""
import re
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
RP = (ROOT / "gamemodes/includes/rp_chat.inc").read_text(encoding="utf-8")
HELP = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
DOCS = (ROOT / "docs/RP_COMMANDS.md").read_text(encoding="utf-8")

class NormalLocalChatTests(unittest.TestCase):
    def test_normal_chat_is_local_only(self):
        match = re.search(r"public OnPlayerText\(playerid, text\[\]\)\s*\{([\s\S]*?)\n\}", GAME)
        self.assertIsNotNone(match)
        callback = match.group(1)
        self.assertIn("RP_SendLocal(playerid, COLOR_WHITE, message, RP_RADIUS_SPEECH);", callback)
        self.assertNotIn("SendClientMessageToAll", callback)
        self.assertIn('format(message, sizeof(message), "%s(%d): %s"', callback)
        self.assertIn("return 0;", callback)  # suppress built-in global broadcast

    def test_normal_chat_shares_spatial_rules(self):
        self.assertIn("#define RP_RADIUS_SPEECH     (20.0)", RP)
        self.assertIn("GetPlayerVirtualWorld(i) != world", RP)
        self.assertIn("GetPlayerInterior(i) != interior", RP)
        self.assertIn("maxDistSquared = radius * radius", RP)
        self.assertIn("GetPlayerPos(i, px, py, pz)", RP)
        self.assertIn("SendClientMessage(i, color, message)", RP)

    def test_guards_and_escaping_preserved(self):
        callback = GAME[GAME.index("public OnPlayerText("):]
        for token in ("gAuthState[playerid] != AUTH_READY", "Guard_AllowChat(playerid)",
                      "gAdminMuteUntil[playerid] > gettime()",
                      "cleanText[n] == '{'", "cleanText[n] == '}'"):
            self.assertIn(token, callback)
        self.assertIn("/help rp", HELP)
        self.assertIn("Обычный текст без команды", DOCS)
        GAME.encode("cp1251", errors="strict")

if __name__ == "__main__":
    unittest.main()
