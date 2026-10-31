"""PLAYER-COLOR-01: organization palette, civilian transparency and lifecycle."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
LEADER = (ROOT / "gamemodes/includes/leader_system.inc").read_text(encoding="utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")


class PlayerFactionColors(unittest.TestCase):
    def test_all_eight_factions_have_distinct_opaque_colors(self):
        match = re.search(
            r"stock Leader_ColorForOrg\(org\)\s*\{(.*?)return PLAYER_COLOR_CIVILIAN;",
            LEADER, re.S
        )
        self.assertIsNotNone(match)
        palette = dict(re.findall(r"case (\d+): return (0x[0-9A-Fa-f]{8});", match.group(1)))
        self.assertEqual(set(palette), set(map(str, range(1, 9))))
        self.assertEqual(len(set(palette.values())), 8)
        self.assertTrue(all(value.endswith("FF") for value in palette.values()))

    def test_civilians_have_neutral_semtransparent_radar_markers(self):
        self.assertIn("#define PLAYER_COLOR_CIVILIAN (0xAAB2BD77)", LEADER)
        self.assertIn("return PLAYER_COLOR_CIVILIAN;", LEADER)
        self.assertIn("SetPlayerColor(playerid, Leader_ColorForOrg(org));", LEADER)

    def test_no_organization_leak_before_login_or_on_disabled_org(self):
        self.assertIn("gAuthState[playerid] != AUTH_READY || !Leader_OrgValid(org)", LEADER)
        self.assertIn("new org = gFactionCachedOrg[playerid];", LEADER)
        self.assertIn("if (!IsPlayerConnected(playerid)) return 0;", LEADER)

    def test_update_on_authorization_join_leave_and_spawn(self):
        refresh = LEADER.split("stock Leader_RefreshPlayer(playerid)", 1)[1].split(
            "stock Leader_RefreshAccount", 1
        )[0]
        self.assertEqual(refresh.count("Leader_ApplyPlayerColor(playerid);"), 2)
        self.assertIn("Leader_RefreshAccount(name);", LEADER)
        self.assertIn("Leader_OnReady(playerid);", GAME)
        self.assertIn("gAuthState[playerid] = AUTH_INTRO;\n    Leader_ApplyPlayerColor(playerid);", GAME)
        spawn = GAME.split("public OnPlayerSpawn(playerid)", 1)[1].split(
            "public OnPlayerDeath", 1
        )[0]
        self.assertIn("Leader_ApplyPlayerColor(playerid);", spawn)

    def test_radar_enabled_only_for_streamed_nearby_players(self):
        self.assertIn("ShowPlayerMarkers(PLAYER_MARKERS_MODE_STREAMED);", GAME)
        self.assertNotIn("ShowPlayerMarkers(PLAYER_MARKERS_MODE_OFF);", GAME)


if __name__ == "__main__":
    unittest.main()
