"""AFK-01: automatic 3D away indicator lifecycle and performance contracts."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
AFK = (ROOT / "gamemodes/includes/afk_status.inc").read_text(encoding="utf-8")
DOCS = (ROOT / "docs/AFK_STATUS.md").read_text(encoding="utf-8")


class AfkIndicatorContracts(unittest.TestCase):
    def test_gamemode_lifecycle_is_wired(self):
        self.assertIn('#include "afk_status.inc"', GAME)
        for hook in ("AFK_Init();", "AFK_Shutdown();",
                     "AFK_OnSpawn(playerid);", "AFK_OnUpdate(playerid);"):
            self.assertIn(hook, GAME)
        self.assertEqual(GAME.count("AFK_Reset(playerid);"), 3)
        self.assertIn("public OnPlayerUpdate(playerid)", GAME)
        self.assertIn("public OnPlayerDeath(playerid", GAME)

    def test_detect_pause_not_motionless_player(self):
        self.assertIn("#define AFK_TIMEOUT_SECONDS       (30)", AFK)
        self.assertIn("gAFKLastUpdate[playerid] = gettime();", AFK)
        self.assertIn("new elapsed = now - gAFKLastUpdate[i];", AFK)
        self.assertIn("if (elapsed < AFK_TIMEOUT_SECONDS)", AFK)
        self.assertNotIn("GetPlayerVelocity", AFK)
        self.assertNotIn("GetPlayerKeys", AFK)
        for guard in ("gAuthState[playerid] == AUTH_READY",
                      "gAccountSpawned[playerid]", "!gAccountDead[playerid]",
                      "!IsPlayerNPC(playerid)", "PLAYER_STATE_SPECTATING"):
            self.assertIn(guard, AFK)

    def test_3d_label_created_once_and_updated_on_timer(self):
        self.assertIn("#define AFK_UPDATE_INTERVAL_MS    (5000)", AFK)
        self.assertEqual(len(re.findall(r'SetTimer\(', AFK)), 1)
        self.assertNotIn("SetTimerEx(", AFK)
        self.assertIn('SetTimer("AFK_Tick", AFK_UPDATE_INTERVAL_MS, true)', AFK)
        self.assertIn("Create3DTextLabel(labelText,", AFK)
        self.assertIn("Attach3DTextLabelToPlayer(gAFKLabel[i], i, 0.0, 0.0, 0.95)", AFK)
        self.assertIn("Update3DTextLabelText(gAFKLabel[i], AFK_LABEL_COLOR, labelText);", AFK)
        self.assertIn("gAFKLabelWorld[i] != world", AFK)
        self.assertIn("Delete3DTextLabel(gAFKLabel[playerid]);", AFK)
        self.assertIn("elapsed / 3600, (elapsed / 60) % 60, elapsed % 60", AFK)

    def test_return_immediately_removes_label_and_no_packet_formatting(self):
        per_packet = AFK.split("stock AFK_OnUpdate(playerid)", 1)[1].split(
            "public AFK_Tick()", 1
        )[0]
        self.assertIn("AFK_ClearLabel(playerid);", per_packet)
        self.assertNotIn("format(", per_packet)
        self.assertNotIn("Create3DTextLabel(", per_packet)
        self.assertIn("if (gAFKTimer) KillTimer(gAFKTimer);", AFK)

    def test_documentation_and_encodings(self):
        for token in ("30 секунд", "5 секунд", "AFK", "виртуальн",
                      "после возвращения", "дисконнекте"):
            self.assertIn(token.casefold(), DOCS.casefold())
        GAME.encode("cp1251", errors="strict")
        AFK.encode("cp1251", errors="strict")


if __name__ == "__main__":
    unittest.main()
