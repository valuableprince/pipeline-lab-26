"""RACE-EVENT-01: динамические опасности и контракт изоляции."""
from pathlib import Path
from build_contracts import assert_staged
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
EVENT = (ROOT / "gamemodes/includes/race_event.inc").read_text(encoding="utf-8")
CORE = (ROOT / "gamemodes/includes/race_core.inc").read_text(encoding="utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
DOC = (ROOT / "docs/RACE_EVENT.md").read_text(encoding="utf-8")


class RaceEventTest(unittest.TestCase):
    def test_personal_objects_only_and_bounded(self):
        self.assertIn("#define RACE_EVENT_COUNT           (6)", EVENT)
        self.assertIn("CreatePlayerObject(playerid, RACE_EVENT_MODEL", EVENT)
        self.assertIn("DestroyPlayerObject(playerid, gRaceEventObjects[playerid][i]);", EVENT)
        self.assertIn("gRaceEventObjects[MAX_PLAYERS][RACE_EVENT_COUNT]", EVENT)
        self.assertNotIn("CreateObject(", EVENT)
        self.assertNotIn("SetTimer(", EVENT)
        self.assertIn("IsValidPlayerObject(playerid", EVENT)
        self.assertIn("RaceEvent_Unload(playerid);\n            return false;", EVENT)
        EVENT.encode("cp1251")

    def test_positions_on_sprint_section(self):
        coords = re.findall(r"\{(\d+\.\d+), (\d+\.\d+), (\d+\.\d+),\s*(-?\d+\.\d+)\}", EVENT)
        self.assertEqual(len(coords), 6)
        for x, y, z, shift in coords:
            x, y, z, shift = map(float, (x, y, z, shift))
            self.assertTrue(700 <= x <= 770)
            self.assertTrue(815 <= y <= 845)
            self.assertTrue(815 <= y + shift <= 845)
            self.assertEqual(z, 166.0)
            self.assertEqual(abs(shift), 3.0)

    def test_phase_not_every_tick(self):
        self.assertIn("new phase = (elapsed / RACE_EVENT_CYCLE_SEC) % 2;", EVENT)
        self.assertIn("if (phase == gRaceEventPhase[playerid]) return 1;", EVENT)
        self.assertIn("MovePlayerObject(playerid, gRaceEventObjects[playerid][i]", EVENT)
        self.assertIn("RaceEvent_Tick(playerid, gettime() - gRaceStartAt);", CORE)

    def test_checkpoint_warns_once(self):
        self.assertIn("gRaceEventWarned[playerid] = true;", EVENT)
        self.assertIn("nextCheckpoint < 9 || gRaceEventWarned[playerid]", EVENT)
        self.assertIn("RaceEvent_OnCheckpoint(playerid, gRaceProgress[playerid]);", CORE)
        self.assertIn("GameTextForPlayer(playerid", EVENT)

    def test_transaction_cleanup(self):
        self.assertIn("if (!RaceEvent_Load(playerid))", CORE)
        self.assertIn("RaceMap_Unload(playerid);", CORE)
        self.assertIn("RaceEvent_Unload(playerid);", CORE)
        self.assertIn("RaceEvent_ResetPlayer(playerid);", CORE)
        self.assertIn('#include "race_map.inc"\n#include "race_event.inc"', MAIN)
        self.assertIn("RACE_WORLD_ID", CORE)
        assert_staged(self, "race_event.inc")
        self.assertIn("обязателен UAT", DOC.lower().replace("обязателен uat", "обязателен UAT"))


if __name__ == "__main__":
    unittest.main()
