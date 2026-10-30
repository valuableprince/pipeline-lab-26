"""RP-CHAT-02: speech gestures are safe and only follow accepted plain chat."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
RP = (ROOT / "gamemodes/includes/rp_chat.inc").read_text(encoding="utf-8")
DOC = (ROOT / "docs/RP_COMMANDS.md").read_text(encoding="utf-8")


class SpeechGestureTests(unittest.TestCase):
    def test_only_plain_chat_triggers_the_speech_animation(self):
        callback = GAME.split("public OnPlayerText(playerid, text[])", 1)[1]
        self.assertIn("RP_PlaySpeechAnimation(playerid);", callback)
        self.assertLess(callback.index("Guard_AllowChat(playerid)"),
                        callback.index("RP_PlaySpeechAnimation(playerid);"))
        self.assertLess(callback.index("gAdminMuteUntil[playerid] > gettime()"),
                        callback.index("RP_PlaySpeechAnimation(playerid);"))
        self.assertLess(callback.index("RP_SendLocal(playerid, COLOR_WHITE, message, RP_RADIUS_SPEECH);"),
                        callback.index("RP_PlaySpeechAnimation(playerid);"))
        self.assertIn("if (hasSpeech) RP_PlaySpeechAnimation(playerid);", callback)
        # Commands are handled by their own routing, not the normal chat callback.
        commands = RP.split("stock RP_Command(playerid", 1)[1]
        self.assertNotIn("RP_PlaySpeechAnimation(", commands)

    def test_standard_speech_clip_is_bounded_and_not_freezing(self):
        helper = RP.split("stock RP_PlaySpeechAnimation(playerid)", 1)[1].split(
            "stock RP_SendLocal", 1
        )[0]
        for value in ('"PED", "IDLE_CHAT"', "4.1, false, false, false,",
                      "false, RP_SPEECH_ANIM_TIME, 0", "#"):
            if value == "#":
                self.assertIn("#define RP_SPEECH_ANIM_TIME (2400)", RP)
            else:
                self.assertIn(value, helper)
        self.assertNotIn("ClearAnimations(", re.sub(r"//[^\\n]*", "", helper))

    def test_safe_conditions_avoid_overriding_other_animations(self):
        helper = RP.split("stock RP_PlaySpeechAnimation(playerid)", 1)[1].split(
            "stock RP_SendLocal", 1
        )[0]
        for guard in ("gAuthState[playerid] != AUTH_READY",
                      "!gAccountSpawned[playerid]", "gAccountDead[playerid]",
                      "gAdminFrozen[playerid]", "TDS_Active(playerid)",
                      "PLAYER_STATE_ONFOOT", "SPECIAL_ACTION_NONE",
                      "GetPlayerVelocity(playerid", "GetPlayerAnimationIndex(playerid)",
                      "GetAnimationName(index", 'strcmp(library, "PED", true)'):
            self.assertIn(guard, helper)

    def test_animation_documented_and_encoding_preserved(self):
        self.assertIn("RP-CHAT-02", DOC)
        self.assertIn("2400", DOC)
        GAME.encode("cp1251", errors="strict")
        RP.encode("cp1251", errors="strict")


if __name__ == "__main__":
    unittest.main()
