"""RACE-MAP-01: контракт картографического модуля и геометрии трассы."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAP = (ROOT / "gamemodes/includes/race_map.inc").read_text(encoding="utf-8")
CORE = (ROOT / "gamemodes/includes/race_core.inc").read_text(encoding="utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")


class RaceMapTest(unittest.TestCase):
    @staticmethod
    def deck_tiles():
        tiles = set()
        for a, b, c, d in re.findall(
            r"RaceMap_Deck\((-?\d+), (-?\d+), (-?\d+), (-?\d+)\)", MAP
        ):
            for x in range(int(a), int(b) + 1):
                for y in range(int(c), int(d) + 1):
                    tiles.add((x, y))
        return tiles

    def test_world_and_lifecycle(self):
        for module in ("race_map", "race_event", "race_playlist", "race_audio", "race_progress", "race_core"):
            self.assertIn('#include "' + module + '.inc"', MAIN)
        self.assertIn("SetPlayerVirtualWorld(playerid, RACE_WORLD_ID);", CORE)
        self.assertIn("SetVehicleVirtualWorld(vehicle, RACE_WORLD_ID);", CORE)
        for token in ("RaceMap_Init()", "RaceMap_Load(playerid)",
                      "RaceMap_Unload(playerid)", "RaceMap_Shutdown()"):
            self.assertIn(token, CORE)
        self.assertIn("CreateDynamicObject(", MAP)
        self.assertIn("DestroyDynamicObject(", MAP)
        self.assertIn("IsValidDynamicObject(objectid)", MAP)
        self.assertNotIn("CreatePlayerObject(", MAP)
        self.assertNotIn("DestroyPlayerObject(", MAP)
        self.assertIn("RaceMap_DestroyGeometry();\n            return false;", MAP)
        self.assertIn("RACE_WORLD_ID, 0, -1, RACE_MAP_STREAM_DISTANCE", MAP)
        self.assertIn("Streamer_Update(playerid);", CORE)

    def test_object_ids_are_official(self):
        objectids = {int(i) for i in re.findall(r"RaceMap_AddObject\((\d+),", MAP)}
        self.assertEqual(objectids, {18765, 18766, 1633, 3798, 18761})
        self.assertIn("RACE_MAP_MAX_OBJECTS      (440)", MAP)
        MAP.encode("cp1251")

    def test_checkpoints_are_on_deck(self):
        tiles = self.deck_tiles()
        self.assertEqual(len(tiles), 290)
        cps = re.findall(r"\{(\d+\.\d+), (\d+\.\d+), 166\.5\}", CORE)
        self.assertEqual(len(cps), 14)
        for x, y in cps:
            with self.subTest(x=x, y=y):
                self.assertIn((round((float(x) - 450.0) / 10.0),
                               round((float(y) - 780.0) / 10.0)), tiles)
        self.assertIn("new Float:x = RACE_MAP_X + 2.0", CORE)
        for slot in range(8):
            x = (2.0 - (slot // 2) * 8.0) / 10.0
            y = (-4.0 + (slot % 2) * 8.0) / 10.0
            self.assertIn((round(x), round(y)), tiles)

    def test_bounded_and_idempotent(self):
        self.assertIn("if (gRaceMapTiles[ix][iy]) return true;", MAP)
        self.assertIn("if (gRaceMapObjectCount >= RACE_MAP_MAX_OBJECTS)", MAP)
        self.assertIn("gRaceMapDynamicObjects[RACE_MAP_MAX_OBJECTS]", MAP)
        self.assertIn("gRaceMapDynamicCount = 0;", MAP)
        self.assertNotIn("gRaceMapPlayerObjects", MAP)
        self.assertNotIn("gRaceMapPlayerCount", CORE)
        self.assertIn('RACE_MAP_SURFACE_Z - 30.0', CORE)
        self.assertIn("gRaceMapReady = RaceMap_Init() != 0;", CORE)

    def test_build_stages_map_include(self):
        for p in ("tools/build_windows.py", ".github/workflows/build.yml"):
            content = (ROOT / p).read_text(encoding="utf-8")
            self.assertIn('glob("*.inc")', content)


if __name__ == "__main__":
    unittest.main()
