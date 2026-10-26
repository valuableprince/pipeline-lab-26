"""Contract checks for the isolated developer house interior preview."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
HOUSE = (ROOT / "gamemodes/includes/house_preview.inc").read_text(encoding="utf-8")


class HousePreviewContract(unittest.TestCase):
    def test_lifecycle_and_command(self):
        for token in ('#include "house_preview.inc"', "House_Init();",
                      "House_Shutdown();", "House_OnConnect(playerid);",
                      "House_OnSpawn(playerid);"):
            self.assertIn(token, MAIN)
        self.assertIn("cmd:house(playerid, params[])", HOUSE)
        self.assertIn("Studio_Require(playerid)", HOUSE)

    def test_world_and_geometry(self):
        self.assertIn("#define HOUSE_PREVIEW_WORLD    (10)", HOUSE)
        self.assertIn("House_Add(19379, 6000.0, 6000.0, 19.91, 0.0, 90.0, 0.0)", HOUSE)
        self.assertIn("House_Add(19450, 6000.0, 6004.8188", HOUSE)
        self.assertIn("House_Add(19353, 5996.7848, 5995.1812", HOUSE)
        self.assertIn("House_Add(19426, 5999.1964, 5995.1812", HOUSE)
        self.assertIn("House_Add(19353, 6003.2152, 5995.1812", HOUSE)
        self.assertIn("gHouseObjectCount == 26", HOUSE)
        self.assertEqual(HOUSE.count("House_Add(") - 1, 26)

    def test_isolated_and_return_state(self):
        self.assertIn("SetPlayerVirtualWorld(playerid, HOUSE_PREVIEW_WORLD)", HOUSE)
        self.assertIn("gHouseReturnWorld[playerid] = GetPlayerVirtualWorld(playerid);", HOUSE)
        self.assertIn("gHouseReturnInterior[playerid] = GetPlayerInterior(playerid);", HOUSE)
        self.assertIn("SetPlayerInterior(playerid, gHouseReturnInterior[playerid]);", HOUSE)
        self.assertIn("SetPlayerVirtualWorld(playerid, gHouseReturnWorld[playerid]);", HOUSE)
        self.assertIn("if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT)", HOUSE)

    def test_doors_and_cleanup(self):
        self.assertIn("SetDynamicObjectRot(gHouseDoor[door]", HOUSE)
        self.assertNotIn("MoveDynamicObject(", HOUSE)
        self.assertIn("DestroyDynamicObject(gHouseObjects[i]);", HOUSE)
        self.assertIn("gHouseDoorOpen[0] = false;", HOUSE)
        self.assertIn("gHouseDoorOpen[1] = false;", HOUSE)
        self.assertIn("IsPlayerInRangeOfPoint(playerid, 2.5", HOUSE)


if __name__ == "__main__":
    unittest.main()
