"""JOB-TRUCK-03: fixed fleet, employee access and waste transfer contracts."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
OPS=(ROOT/"gamemodes/includes/cleaner_truck_ops.inc").read_text("utf-8")
LOAD=(ROOT/"gamemodes/includes/cleaner_truck_loading.inc").read_text("utf-8")
CMDS=(ROOT/"gamemodes/includes/commands.inc").read_text("utf-8")
MAIN=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
TRUCK=(ROOT/"gamemodes/includes/cleaner_truck.inc").read_text("utf-8")

class JobTruckStageThree(unittest.TestCase):
    def test_permanent_fleet_bays(self):
        self.assertIn("TRUCK_BAYS (2)", OPS)
        self.assertIn("gTruckFleet[i] = CreateVehicle(TRUCK_VEHICLE_MODEL", OPS)
        self.assertIn("gTruckSpawnPositions[i][0]", OPS)
        self.assertIn("SetVehicleToRespawn(vehicleid);", OPS)
        self.assertIn("gTruckBayOwner[bay] = playerid + 1;", OPS)
        self.assertIn("new gTruckCargo[TRUCK_BAYS];", OPS)
        self.assertNotIn("stock bool:Truck_IssueVehicle(", OPS)

    def test_municipal_access_control(self):
        self.assertIn("Truck_FleetBay(vehicleid)", OPS)
        self.assertIn("!gCleanerDuty[playerid] && !gTruckOnShift[playerid]", OPS)
        self.assertIn("newstate == PLAYER_STATE_DRIVER", OPS)
        self.assertIn("gTruckBayOwner[bay] != playerid + 1", OPS)
        self.assertIn("RemovePlayerFromVehicle(playerid);", OPS)
        self.assertIn("stock Truck_OnEnterVehicle(playerid, vehicleid, ispassenger)", OPS)
        self.assertIn("ClearAnimations(playerid);", OPS)
        self.assertIn("return Truck_OnEnterVehicle(playerid, vehicleid, ispassenger);", MAIN)

    def test_animation_and_anti_replay(self):
        for token in ("MoveObject(", "StopObject(", "SetObjectPos(", "SetObjectRot(",
                      'SetTimerEx("TruckLoad_Step"', "gTruckLoadEpoch[playerid]++",
                      "epoch != gTruckLoadEpoch[playerid]", "TRUCK_PHASE_LIFT",
                      "TRUCK_PHASE_TIP", "TRUCK_PHASE_RETURN",
                      "TRUCK_PHASE_THROW_UP", "TRUCK_PHASE_THROW_IN"):
            self.assertIn(token, LOAD)
        self.assertIn("if (!TruckLoad_Commit(playerid, slot, false, gTruckLoadBinUnits[playerid]))", LOAD)
        self.assertIn("if (!TruckLoad_Commit(playerid, slot, true, units))", LOAD)

    def test_authoritative_transfer_and_no_early_payment(self):
        for token in ("DBPrimary_Execute(query, changed)", "changed != 1",
                      "gCleanerDumpBin[slot] -= amount",
                      "gCleanerDumpOverflow[slot] -= amount",
                      "gTruckCargo[bay] += amount",
                      "gCleanerDumpStaged", "TruckLoad_Parked(playerid)"):
            self.assertIn(token, LOAD)
        self.assertNotIn("GivePlayerMoney(", LOAD)
        self.assertNotIn("Economy_Credit(", LOAD)
        self.assertNotIn("Economy_Debit(", LOAD)

    def test_bag_pickup_throw_and_cancel(self):
        for token in ("TRUCK_BAG_UNITS (4)", "gTruckHeldUnits[playerid] = units",
                      "DestroyObject(gCleanerDumpBags[slot][j]);",
                      "SetPlayerAttachedObject(", "RemovePlayerAttachedObject(",
                      "gTruckThrowObject[playerid]",
                      "CleanerDump_Refresh(slot);", "TruckLoad_Reset(playerid);"):
            self.assertIn(token, LOAD)
        for name in ("truckload", "truckbag", "truckthrow", "truckcargo"):
            self.assertIn(f"cmd:{name}(playerid, params[])", CMDS)
        self.assertIn("TruckLoad_HandleKey(playerid)", TRUCK)

    def test_driver_can_switch_to_other_free_municipal_truck(self):
        self.assertIn("stock bool:Truck_SelectVehicle(playerid, bay)", OPS)
        self.assertIn("gTruckBayOwner[bay] != playerid + 1", OPS)
        self.assertIn("TruckLoad_CurrentBusy(playerid)", OPS)
        self.assertIn("gTruckBayOwner[oldBay] = 0;", OPS)
        self.assertIn("gTruckVehicle[playerid] = gTruckFleet[bay];", OPS)
        self.assertIn("(!ispassenger && !Truck_SelectVehicle(playerid, bay))", OPS)
        self.assertIn("!Truck_SelectVehicle(playerid, bay)", OPS)
        self.assertIn("!TruckLoad_CurrentBusy(playerid)", OPS)

    def test_loading_clears_truck_before_crossing_and_returns_high(self):
        self.assertIn("TRUCK_HIGH_CLEARANCE (3.0)", LOAD)
        self.assertIn("TRUCK_PHASE_LOWER (6)", LOAD)
        self.assertIn("gTruckLoadHighZ[playerid] = tz + TRUCK_HIGH_CLEARANCE;", LOAD)
        self.assertIn("gTruckLoadHighZ[playerid], 1.6, 110.0", LOAD)
        self.assertIn("gTruckLoadHighZ[playerid],", LOAD)
        self.assertIn("if (phase == TRUCK_PHASE_LOWER)", LOAD)
        self.assertNotIn("MoveObject(gCleanerDumpObject[slot], x, y, z + 2.1", LOAD)

    def test_inflight_commit_does_not_recheck_exact_park_position(self):
        self.assertIn("stock bool:TruckLoad_ValidSession(playerid)", LOAD)
        self.assertIn("TRUCK_LOAD_MAX_DRIFT_SQ (36.0)", LOAD)
        commit = LOAD.split("stock bool:TruckLoad_Commit(", 1)[1].split(
            "stock TruckLoad_FinishIfEmpty(", 1)[0]
        self.assertIn("!TruckLoad_ValidSession(playerid)", commit)
        self.assertNotIn("!TruckLoad_Parked(playerid)", commit)
        self.assertIn("epoch != gTruckLoadEpoch[playerid]", LOAD)
        self.assertNotIn("Погрузка отменена: данные контейнера изменились.", LOAD)

    def test_file_encoding(self):
        for text in (OPS,LOAD,CMDS,MAIN,TRUCK):
            self.assertEqual(text.encode("cp1251").decode("cp1251"), text)

if __name__ == "__main__":
    unittest.main()
