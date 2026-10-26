"""Contract checks for server-side Big Ear automatic/manual rotation."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
DISH = (ROOT / "gamemodes/includes/big_ear.inc").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")


class BigEarRotation(unittest.TestCase):
    def test_lifecycle(self):
        for token in ('#include "big_ear.inc"', "Dish_Init();",
                      "Dish_Shutdown();", "Dish_OnConnect(playerid);",
                      "Dish_OnObjectMoved(objectid);"):
            self.assertIn(token, MAIN)
        self.assertIn("public OnObjectMoved(objectid)", MAIN)

    def test_world_model_replacement(self):
        self.assertIn("DISH_MODEL_ID          (16613)", DISH)
        self.assertIn("CreateObject(DISH_MODEL_ID", DISH)
        self.assertIn("RemoveBuildingForPlayer(playerid, -1", DISH)
        self.assertIn("if (!Dish_Ready()) return 0;", DISH)

    def test_automatic_motion_and_limits(self):
        for token in ("DISH_ROT_LIMIT", "DISH_AUTO_STEP", "DISH_MANUAL_SPEED",
                      "MoveObject(gDishObject", "Dish_AdvanceAutomatic()",
                      "gDishDirection = -gDishDirection;",
                      "StopObject(gDishObject);"):
            self.assertIn(token, DISH)
        self.assertNotIn("SetTimer(", DISH)

    def test_manual_permissions(self):
        self.assertIn("stock Dish_Command(playerid, const params[])", DISH)
        self.assertIn("cmd:dish(playerid, params[])", COMMANDS)
        self.assertIn("return Dish_Command(playerid, params);", COMMANDS)
        self.assertIn("[BIG-EAR] /dish received.", COMMANDS)
        self.assertIn("Admin_Require(playerid, PERM_MANAGE_STAFF)", DISH)
        for token in ('"auto"', '"stop"', '"status"', '"angle"', '"left"', '"right"'):
            self.assertIn(token, DISH)
        self.assertIn("value < -DISH_ROT_LIMIT || value > DISH_ROT_LIMIT", DISH)
        self.assertIn("value < 1.0 || value > 90.0", DISH)

    def test_no_unrelated_movement_or_database(self):
        self.assertNotIn("SetPlayerPos(", DISH)
        self.assertNotIn("mysql_", DISH)


if __name__ == "__main__":
    unittest.main()
