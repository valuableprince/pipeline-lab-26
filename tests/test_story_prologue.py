"""Room 17 playable prologue contracts."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
STORY = (ROOT / "gamemodes/includes/story_prologue.inc").read_text("utf-8")
DB = (ROOT / "gamemodes/includes/db_primary.inc").read_text("utf-8")
SQL = (ROOT / "db/migrations/008_story_prologue.sql").read_text("utf-8")
CMDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")


class Room17Contract(unittest.TestCase):
    def test_story_entrypoints(self):
        self.assertIn('#include "story_prologue.inc"', MAIN)
        for entry in ("Story_OnConnect(playerid);", "Story_OnDisconnect(playerid);",
                      "Story_OnSpawn(playerid);",
                      "Story_OnKeyStateChange(playerid, newkeys, oldkeys)",
                      "Story_OnDialog(playerid, dialogid, response)"):
            self.assertIn(entry, MAIN)
        self.assertIn("return Story_Command(playerid);", CMDS)
        self.assertNotIn("Полноценный журнал расследования появится позже", CMDS)

    def test_persistence_safe_progress(self):
        self.assertIn("ADD COLUMN story_stage", SQL)
        self.assertIn("VALUES (8)", SQL)
        self.assertNotIn("DROP TABLE", SQL)
        self.assertIn("WHERE version=9 LIMIT 1", DB)
        self.assertIn('"story_stage", storyStage', DB)
        self.assertIn("gStoryStage[playerid] = storyStage", DB)
        self.assertIn("row_version=row_version+1", STORY)
        self.assertIn("AND story_stage=%d", STORY)
        self.assertIn("DBPrimary_Execute(q, affected)", STORY)
        self.assertIn("affected != 1", STORY)
        self.assertIn("gDBPrimaryVersion[playerid]++", STORY)
        self.assertLess(STORY.index("DBPrimary_Execute(q, affected)"),
                        STORY.index("gStoryStage[playerid] = expected + 1"))

    def test_private_location_and_safe_interaction(self):
        self.assertIn("CreatePlayer3DTextLabel", STORY)
        self.assertIn("DeletePlayer3DTextLabel", STORY)
        self.assertIn("SetPlayerMapIcon", STORY)
        self.assertIn("RemovePlayerMapIcon", STORY)
        self.assertIn("KEY_WALK", STORY)
        self.assertIn("GetPlayerVirtualWorld(playerid) != 0", STORY)
        self.assertIn("GetPlayerState(playerid) != PLAYER_STATE_ONFOOT", STORY)
        self.assertIn("GetTickCount() - gStoryLastInteract[playerid] < 1000", STORY)
        self.assertNotIn("SetPlayerCheckpoint", STORY)
        self.assertNotIn("NPC_Create", STORY)
        self.assertNotIn("GivePlayerMoney", STORY)

    def test_story_progression_and_ending(self):
        for line in ("Стойка", "НОМЕР 17", "РАДИОСИГНАЛ",
                     "НОРА", "КОНТУР", "ПРОЛОГ ЗАВЕРШЁН"):
            self.assertIn(line, STORY)
        self.assertIn("STORY_DONE            (4)", STORY)
        self.assertIn("Story_Advance(playerid, expected)", STORY)


if __name__ == "__main__":
    unittest.main()
