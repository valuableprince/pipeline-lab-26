"""Static audit of unwanted stock race entry checkpoints vs custom races."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
RACE = (ROOT / "gamemodes/includes/race_core.inc").read_text("utf-8")
INCLUDES = ROOT / "gamemodes/includes"
VENDORED = {"Pawn.CMD.inc", "sscanf2.inc", "streamer.inc"}

class LegacyRaceEntriesContract(unittest.TestCase):
    def test_cleanup_on_connection_and_spawn(self):
        for head, end in (("public OnPlayerConnect(playerid)", "public OnPlayerDisconnect"),
                          ("public OnPlayerSpawn(playerid)", "public OnPlayerDeath")):
            part = MAIN.split(head, 1)[1].split(end, 1)[0]
            self.assertIn("DisablePlayerCheckpoint(playerid);", part)
            self.assertIn("DisablePlayerRaceCheckpoint(playerid);", part)

    def test_interior_markers_remain_disabled(self):
        self.assertIn("DisableInteriorEnterExits();", MAIN)

    def test_default_race_icons_and_checkpoints_not_created_in_our_sources(self):
        sources = [ROOT / "gamemodes/signal0317.pwn", *INCLUDES.glob("*.inc")]
        for path in sources:
            if path.name in VENDORED:
                continue
            source = path.read_text("utf-8")
            self.assertNotRegex(source, r"\bSetPlayerCheckpoint\s*\(")
            self.assertNotRegex(source, r"\bSetPlayerMapIcon\s*\([^;]*,\s*(?:33|53)\s*,", path.name)

    def test_tarkenso_custom_race_checkpoints_remain(self):
        self.assertIn("SetPlayerRaceCheckpoint(playerid,", RACE)
        self.assertIn("Race_SetNextCheckpoint(playerid);", RACE)
        self.assertIn("RACE_WORLD_ID", RACE)

if __name__ == "__main__":
    unittest.main()
