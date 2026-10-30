"""RP-ANIM-01: whitelist, roleplay guards, key-to-stop, discoverability."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
ANIM = (ROOT / "gamemodes/includes/rp_animations.inc").read_text(encoding="utf-8")
CHAT = (ROOT / "gamemodes/includes/rp_chat.inc").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
HELP = (ROOT / "gamemodes/includes/player_command_help.inc").read_text(encoding="utf-8")
MENU = (ROOT / "gamemodes/includes/player_menu.inc").read_text(encoding="utf-8")
DOCS = (ROOT / "docs/RP_COMMANDS.md").read_text(encoding="utf-8")


class RpAnimationsContract(unittest.TestCase):
    def test_whitelist_and_ui(self):
        for action in ('"sit"', '"handsup"', '"wave"', '"crossarms"',
                       '"smoke"', '"tired"', '"talk"', '"lie"', '"cower"'):
            self.assertIn(action, ANIM)
        self.assertIn("DIALOG_STYLE_TABLIST_HEADERS", ANIM)
        self.assertIn("RPAnim_OnDialog(playerid, dialogid, response, listitem)", GAME)
        self.assertIn("if (listitem < 0 || listitem >= sizeof(gRPAnimations))", ANIM)
        self.assertIn("for (new i = 0; i < sizeof(gRPAnimations); i++)", ANIM)
        self.assertIn('ApplyAnimation(playerid, gRPAnimations[index][raLibrary]', ANIM)
        self.assertNotIn('ApplyAnimation(playerid, params', ANIM)

    def test_space_key_is_sprint_not_jump_and_owned_only(self):
        self.assertIn("KEY_SPRINT", ANIM)
        keys = ANIM.split("stock RPAnim_OnKeys(", 1)[1]
        self.assertIn("!RPAnim_Active(playerid)", keys)
        self.assertIn("!(newkeys & KEY_SPRINT)", keys)
        self.assertIn("(oldkeys & KEY_SPRINT)", keys)
        self.assertNotIn("KEY_JUMP)", keys)
        self.assertIn("if (RPAnim_Stop(playerid))", keys)
        self.assertIn("if (RPAnim_OnKeys(playerid, newkeys, oldkeys)) return 1;", GAME)
        self.assertIn("GetPlayerAnimationIndex(playerid)", ANIM)
        self.assertIn("RPAnim_IsCurrent(playerid)", ANIM)
        self.assertIn("ClearAnimations(playerid, SYNC_ALL);", ANIM)
        self.assertNotIn("SetTimerEx(", ANIM)

    def test_guards_and_lifecycle(self):
        for s in ("AUTH_READY", "gAccountSpawned[playerid]", "!gAccountDead[playerid]",
                  "!gAdminFrozen[playerid]", "!gAdminFly[playerid]",
                  "TDS_Active(playerid)", "Race_IsParticipant(playerid)",
                  "gCleanerDuty[playerid]", "PLAYER_STATE_ONFOOT",
                  "SPECIAL_ACTION_NONE", "GetPlayerVelocity(playerid"):
            self.assertIn(s, ANIM)
        self.assertEqual(GAME.count("RPAnim_Reset(playerid);"), 4)
        self.assertIn("if (newstate != PLAYER_STATE_ONFOOT) RPAnim_Reset(playerid);", GAME)
        self.assertIn("RPAnim_Active(playerid)", CHAT)
        self.assertIn("RP_SPEECH_ANIM_TIME, SYNC_NONE", CHAT)

    def test_commands_and_help(self):
        for c in ("anim", "sit", "handsup", "wave"):
            self.assertIn(f"cmd:{c}(playerid, params[])", COMMANDS)
            self.assertIn(f'{{"{c}", "/{c}', HELP)
        self.assertIn('/help anim', COMMANDS)
        self.assertIn('!strcmp(category, "anim", true)', COMMANDS)
        self.assertIn('/anim', MENU)
        self.assertIn('ПРОБЕЛ', DOCS)
        for item in (GAME, ANIM, CHAT, COMMANDS, HELP, MENU):
            item.encode("cp1251", errors="strict")


if __name__ == "__main__":
    unittest.main()
