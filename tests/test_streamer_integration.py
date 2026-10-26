"""STREAMER-01: критические контракты интеграции с open.mp."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
STREAMER = (ROOT / "gamemodes/includes/streamer.inc").read_text(encoding="utf-8")
RACE_MAP = (ROOT / "gamemodes/includes/race_map.inc").read_text(encoding="utf-8")
RACE_CORE = (ROOT / "gamemodes/includes/race_core.inc").read_text(encoding="utf-8")
DOC = (ROOT / "docs/JOB_CLEANER_05.md").read_text(encoding="utf-8")


class StreamerIntegrationTests(unittest.TestCase):
    def test_versioned_native_include_and_build(self):
        self.assertIn('#include "streamer.inc"', GAME)
        for token in ("CreateDynamicObject(", "DestroyDynamicObject(",
                      "CreateDynamic3DTextLabel(", "Streamer_Update("):
            self.assertIn(token, STREAMER)
        self.assertIn("Licensed under the Apache License, Version 2.0", STREAMER)
        for path in (ROOT / "tools/build_windows.py",
                     ROOT / ".github/workflows/build.yml"):
            self.assertIn('"streamer.inc"', path.read_text(encoding="utf-8"))

    def test_world_isolation_and_single_registration(self):
        self.assertIn("#define RACE_WORLD_ID            (1800)", RACE_MAP)
        self.assertNotIn("#define RACE_WORLD_ID", RACE_CORE)
        self.assertIn("RACE_WORLD_ID, 0, -1, RACE_MAP_STREAM_DISTANCE", RACE_MAP)
        self.assertIn("RaceMap_CreateGeometry()", RACE_MAP)
        self.assertIn("RaceMap_DestroyGeometry()", RACE_MAP)
        self.assertIn("Streamer_Update(playerid);", RACE_CORE)
        unloading = RACE_MAP.split("stock RaceMap_Unload(playerid)")[1].split("stock bool:RaceMap_Load")[0]
        self.assertNotIn("DestroyDynamicObject(", unloading)
        self.assertNotIn("CreatePlayerObject(", RACE_MAP)
        self.assertNotIn("DestroyPlayerObject(", RACE_MAP)

    def test_install_documentation_and_encoding(self):
        for token in ("v2.9.6", "pawn.legacy_plugins", "samp_bcrypt",
                      "streamer.dll", "streamer.so", "UAT"):
            self.assertIn(token, DOC)
        RACE_MAP.encode("cp1251")


if __name__ == "__main__":
    unittest.main()
