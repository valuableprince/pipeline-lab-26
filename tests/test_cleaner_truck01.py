"""JOB-TRUCK-01: menu, rank gate, current DB-backed container orders."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
TRUCK = (ROOT/"gamemodes/includes/cleaner_truck.inc").read_text("utf-8")
JOB = (ROOT/"gamemodes/includes/cleaner_job.inc").read_text("utf-8")
COMMANDS = (ROOT/"gamemodes/includes/commands.inc").read_text("utf-8")

class TruckStageOneTests(unittest.TestCase):
    def test_wiring(self):
        for token in ('#include "cleaner_truck.inc"', 'Truck_OnKeyStateChange(', 'Truck_OnDialog(', 'Truck_Reset(playerid);'):
            self.assertIn(token, GAME)
        self.assertIn("cmd:orders(playerid, params[])", COMMANDS)
        self.assertIn("return Truck_ShowOrders(playerid);", COMMANDS)

    def test_rank_and_specialization(self):
        self.assertIn("if (Cleaner_Rank(playerid) < 2)", TRUCK)
        self.assertIn("if (xp >= 20) return 2;", JOB)
        self.assertIn("gTruckOnShift[playerid] = true;", TRUCK)
        self.assertIn("Cleaner_OnKeyStateChange(playerid, KEY_WALK, KEY:0)", TRUCK)

    def test_order_data_and_location(self):
        for token in ("gCleanerDumpId[i]", "gCleanerDumpPos[i][0]", "gCleanerDumpBin[i] + gCleanerDumpOverflow[i]", "Truck_LocationName(", "TRUCK_ORDER_THRESHOLD (60)", "Fort Carson", "Las Payasadas"):
            self.assertIn(token, TRUCK)
        self.assertIn("gTruckDialogContainer[playerid][gTruckDialogCount[playerid]++] = gCleanerDumpId[i];", TRUCK)
        self.assertIn("gCleanerDumpId[i] != id", TRUCK)
        self.assertIn("TRUCK_MAP_SLOT (74)", TRUCK)

    def test_encoding(self):
        self.assertEqual(TRUCK, TRUCK.encode("cp1251").decode("cp1251"))

if __name__ == "__main__":
    unittest.main()
