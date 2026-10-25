"""BROADCAST-01: static and integration-contract regressions."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
BROADCAST = (ROOT / "gamemodes/includes/broadcast.inc").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
ADMIN = (ROOT / "gamemodes/includes/admin_system.inc").read_text(encoding="utf-8")
HELP = (ROOT / "gamemodes/includes/admin_help.inc").read_text(encoding="utf-8")
RADIO = (ROOT / "gamemodes/includes/radio_player.inc").read_text(encoding="utf-8")
RACE = (ROOT / "gamemodes/includes/race_audio.inc").read_text(encoding="utf-8")
STORY = (ROOT / "gamemodes/includes/story_prologue.inc").read_text(encoding="utf-8")
GATEWAY = (ROOT / "services/broadcast/server.js").read_text(encoding="utf-8")
BROWSER = (ROOT / "services/broadcast/public/app.js").read_text(encoding="utf-8")


class BroadcastContract(unittest.TestCase):
    def test_pawn_codec(self):
        BROADCAST.encode("cp1251")

    def test_compose_main(self):
        self.assertIn('#include "broadcast.inc"', MAIN)
        for name in ("Broadcast_OnConnect", "Broadcast_OnDisconnect",
                     "Broadcast_OnReady"):
            self.assertIn(name + "(playerid)", MAIN)
        self.assertIn("Broadcast_Init();", MAIN)
        self.assertIn("Broadcast_Shutdown();", MAIN)
        self.assertIn("Broadcast_OnDialog(playerid, dialogid, response, listitem)", MAIN)
        self.assertIn("Broadcast_OnDutyDisabled(playerid)", ADMIN)

    def test_admin_min_level_and_founder_priority(self):
        self.assertIn("Admin_Require(playerid, PERM_MANAGE_STAFF)", BROADCAST)
        self.assertIn("Admin_IsFounder(playerid)", BROADCAST)
        self.assertIn('gBroadcastOwner != playerid && Admin_IsFounder(playerid)', BROADCAST)
        self.assertIn("gBroadcastOwnerToken = gAuthToken[playerid]", BROADCAST)
        self.assertIn("gAuthToken[playerid] != gBroadcastOwnerToken", BROADCAST)
        self.assertIn('Admin_LogPlayer(playerid, "BROADCAST_START"', BROADCAST)
        self.assertIn("if (!Admin_LogPlayer", BROADCAST)
        self.assertIn("cmd:broadcast(playerid, params[])", COMMANDS)
        self.assertIn('{"broadcast", "/broadcast [start|stop|status]"', HELP)

    def test_fail_closed_config(self):
        self.assertIn('"broadcast.cfg"', BROADCAST)
        self.assertIn('"https://audio.tarkenso.ru/"', BROADCAST)
        self.assertIn('if (!gBroadcastUrl[0])', BROADCAST)
        self.assertIn('if (fread(config, extra, sizeof(extra))) valid = false;', BROADCAST)
        self.assertNotIn('PlayAudioStreamForPlayer(playerid, "https://', BROADCAST)

    def test_time_limits_and_disconnection(self):
        self.assertIn("#define BROADCAST_DURATION     (120)", BROADCAST)
        self.assertIn("#define BROADCAST_COOLDOWN     (300)", BROADCAST)
        self.assertIn('SetTimer("Broadcast_Tick", BROADCAST_TICK_MS, true)', BROADCAST)
        self.assertIn('gBroadcastEndsAt = gettime() + BROADCAST_DURATION', BROADCAST)
        self.assertIn('gBroadcastOwner == playerid)', BROADCAST)
        self.assertIn("Broadcast_Stop(\"ведущий отключился\")", BROADCAST)

    def test_one_audio_owner_and_resume(self):
        self.assertIn("Broadcast_IsActiveForPlayer(playerid)", RADIO)
        self.assertIn("Broadcast_IsActiveForPlayer(playerid)", RACE)
        self.assertIn("Broadcast_IsActiveForPlayer(playerid)", STORY)
        self.assertIn("gBroadcastPreviousRadio[playerid] = gRadioStation[playerid]", BROADCAST)
        self.assertIn("Radio_Play(playerid, gBroadcastPreviousRadio[playerid])", BROADCAST)
        self.assertIn("RaceAudio_Play(playerid, stage)", BROADCAST)
        self.assertIn("gBroadcastAttempted[playerid]", BROADCAST)

    def test_web_security_and_codec(self):
        for token in ("BROADCAST_PUBLIC_ORIGIN", "BROADCAST_ADMIN_TOKEN",
                      "ICECAST_SOURCE_PASSWORD", "timingSafeEqual", "maxPayload",
                      "authToken(req)", "validOrigin(req)", "'-f', 'webm'",
                      "'libmp3lame'", "shell: false", "maxMs = 120_000"):
            self.assertIn(token, GATEWAY)
        self.assertIn("MediaRecorder.isTypeSupported('audio/webm;codecs=opus')", BROWSER)
        self.assertIn("getUserMedia", BROWSER)
        self.assertIn("WebSocket", BROWSER)


if __name__ == "__main__":
    unittest.main()
