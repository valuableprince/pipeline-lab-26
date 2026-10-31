"""RADAR-02: faction radar blips are per-viewer, not the local player arrow."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
LEADER = (ROOT / "gamemodes/includes/leader_system.inc").read_text(encoding="utf-8")
DOCS = (ROOT / "docs/PLAYER_COLOR_SCHEME.md").read_text(encoding="utf-8")


class RadarFactionSyncTests(unittest.TestCase):
    def test_crime_org_six_uses_same_copper_color_as_nametag(self):
        self.assertIn("case 6: return 0xB87949FF;", LEADER)
        self.assertIn("SetPlayerColor(playerid, Leader_ColorForOrg(org));", LEADER)
        self.assertIn("SetPlayerMarkerForPlayer(viewerid, targetid, Leader_ColorForOrg(org));", LEADER)

    def test_faction_change_pushes_blip_color_to_observers(self):
        method = LEADER.split("stock Leader_ApplyPlayerColor(playerid)", 1)[1].split(
            "stock Leader_FilePath(", 1
        )[0]
        self.assertIn("SetPlayerColor(playerid, Leader_ColorForOrg(org));", method)
        self.assertIn("for (new viewerid = 0; viewerid < MAX_PLAYERS; viewerid++)", method)
        self.assertIn("Leader_RefreshRadarMarker(viewerid, playerid);", method)
        self.assertIn("Leader_ApplyPlayerColor(playerid);", LEADER)
        self.assertIn("Leader_RefreshAccount(const name[])", LEADER)

    def test_stream_in_restores_marker_for_reappearing_player(self):
        self.assertIn("public OnPlayerStreamIn(playerid, forplayerid)", GAME)
        part = GAME.split("public OnPlayerStreamIn(playerid, forplayerid)", 1)[1].split(
            "public OnPlayerUpdate(playerid)", 1
        )[0]
        self.assertIn("Leader_RefreshRadarMarker(forplayerid, playerid);", part)
        self.assertIn("ShowPlayerMarkers(PLAYER_MARKERS_MODE_STREAMED);", GAME)
        self.assertNotIn("ShowPlayerMarkers(PLAYER_MARKERS_MODE_GLOBAL);", GAME)

    def test_no_self_marker_override_or_heavy_per_tick_changes(self):
        method = LEADER.split("stock Leader_RefreshRadarMarker(viewerid, targetid)", 1)[1].split(
            "stock Leader_ApplyPlayerColor(playerid)", 1
        )[0]
        for guard in ("!IsPlayerConnected(viewerid)", "!IsPlayerConnected(targetid)",
                      "IsPlayerNPC(viewerid)", "IsPlayerNPC(targetid)",
                      "viewerid == targetid",
                      "gAuthState[targetid] != AUTH_READY",
                      "!Leader_OrgValid(org)"):
            self.assertIn(guard, method)
        per_tick = GAME.split("public OnPlayerUpdate(playerid)", 1)[1].split(
            "public OnPlayerClickPlayerTextDraw", 1
        )[0]
        self.assertNotIn("Leader_RefreshRadarMarker", per_tick)
        self.assertNotIn("SetPlayerMapIcon", LEADER)
        self.assertIn("собственную белую стрелку не считать ошибкой", DOCS)

    def test_source_cp1251(self):
        for item in (GAME, LEADER):
            item.encode("cp1251", errors="strict")


if __name__ == "__main__":
    unittest.main()
