"""REGRESSION: no default SA-MP class selector on death, no repeated story ending."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
MAIN=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
STORY=(ROOT/"gamemodes/includes/story_prologue.inc").read_text("utf-8")

class DeathRespawnRegression(unittest.TestCase):
    def test_death_triggers_guarded_auto_respawn(self):
        death=MAIN.split("public OnPlayerDeath(",1)[1].split("public Auth_DeathRespawn(",1)[0]
        self.assertIn("gAccountDead[playerid] = true;",death)
        self.assertIn("Auth_SaveAccount(playerid)",death)
        self.assertIn('SetTimerEx("Auth_DeathRespawn", 1500, false, "ii", playerid, gAuthToken[playerid]);',death)
        fn=MAIN.split("public Auth_DeathRespawn(",1)[1].split("public OnPlayerKeyStateChange(",1)[0]
        for token in ("IsPlayerConnected(playerid)", "gAuthToken[playerid] != token",
                      "gAuthState[playerid] != AUTH_READY", "!gAccountDead[playerid]",
                      "SpawnPlayer(playerid);"):
            self.assertIn(token,fn)

    def test_class_selection_is_bypassed_for_dead_authenticated_player(self):
        selection=MAIN.split("public OnPlayerRequestClass(",1)[1].split("public OnPlayerRequestSpawn(",1)[0]
        self.assertIn("gAuthState[playerid] == AUTH_READY",selection)
        self.assertIn("gAccountSpawned[playerid]",selection)
        self.assertIn("gAccountDead[playerid]",selection)
        self.assertIn("SpawnPlayer(playerid);",selection)
        self.assertLess(selection.index("SpawnPlayer(playerid);"),selection.index("Auth_GetSpawnCoordinates("))

    def test_spawn_cancels_pending_respawn(self):
        spawn=MAIN.split("public OnPlayerSpawn(",1)[1].split("public OnPlayerDeath(",1)[0]
        self.assertIn("gAccountDead[playerid] = false;",spawn)
        self.assertIn("gAuthDeathRespawnPending[playerid] = false;",spawn)
        self.assertIn("if (!AdminPunish_OnSpawn(playerid)) Story_OnSpawn(playerid);",spawn)
        self.assertIn("Auth_GetSpawnCoordinates(playerid, spawnX",spawn)

    def test_completed_story_not_announced_on_each_respawn(self):
        story=STORY.split("stock Story_OnSpawn(playerid)",1)[1].split("stock Story_OnKeyStateChange(",1)[0]
        self.assertIn("if (gStoryStage[playerid] >= STORY_DONE) return 1;",story)
        self.assertLess(story.index(">= STORY_DONE"),story.index("Story_ObjectiveText(playerid,"))
        self.assertIn("Story_UpdateMarker(playerid);",story)

    def test_sources_cp1251(self):
        for src in (MAIN,STORY): self.assertEqual(src,src.encode("cp1251").decode("cp1251"))

if __name__=="__main__": unittest.main()
