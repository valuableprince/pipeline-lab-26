"""RADAR-03: global faction blips via native SetPlayerColor alpha."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
LEADER = (ROOT / "gamemodes/includes/leader_system.inc").read_text(encoding="utf-8")
DOCS = (ROOT / "docs/PLAYER_COLOR_SCHEME.md").read_text(encoding="utf-8")


class RadarFactionSyncTests(unittest.TestCase):
    def test_civilian_translucent_marker_and_white_text(self):
        self.assertIn("#define PLAYER_COLOR_CIVILIAN (0xFFFFFF77)", LEADER)
        self.assertIn("return PLAYER_COLOR_CIVILIAN;", LEADER)
        self.assertIn("ShowPlayerMarkers(PLAYER_MARKERS_MODE_STREAMED);", GAME)

    def test_crime_org_six_uses_copper_color(self):
        self.assertIn("case 6: return 0xB87949FF;", LEADER)
        self.assertIn("SetPlayerColor(playerid, Leader_ColorForOrg(org));", LEADER)

    def test_all_viewers_receive_global_color_on_faction_change(self):
        method = LEADER.split("stock Leader_ApplyPlayerColor(playerid)", 1)[1].split(
            "stock Leader_FilePath(", 1
        )[0]
        self.assertIn("SetPlayerColor(playerid, Leader_ColorForOrg(org));", method)
        self.assertNotIn("SetPlayerMarkerForPlayer", method)
        self.assertIn("Leader_RefreshAccount(const name[])", LEADER)
        self.assertIn("Leader_ApplyPlayerColor(playerid);", LEADER)

    def test_no_personal_marker_races_after_streaming_or_reconnect(self):
        self.assertNotIn("SetPlayerMarkerForPlayer(", LEADER)
        self.assertNotIn("Leader_RefreshRadarMarker", GAME)
        self.assertNotIn("Leader_RefreshRadarForViewer", GAME)
        self.assertNotIn("SetPlayerMapIcon(", LEADER)
        self.assertIn("Штатная стрелка собственного персонажа", DOCS)

    def test_source_cp1251(self):
        for item in (GAME, LEADER):
            item.encode("cp1251", errors="strict")


if __name__ == "__main__":
    unittest.main()
