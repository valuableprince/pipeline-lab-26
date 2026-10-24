"""Admin /veh contracts: many temporary cars, ID deletion and throttling."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
VEH = (ROOT / "gamemodes/includes/admin_vehicle.inc").read_text("utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
CMDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")


class AdminVehicleContract(unittest.TestCase):
    def test_admin_only_and_valid_creation_parameters(self):
        self.assertIn("cmd:veh(playerid, params[])", CMDS)
        self.assertIn("return AdminVeh_Command(playerid, params);", CMDS)
        self.assertIn("Admin_Require(playerid, PERM_REPORT)", VEH)
        self.assertIn('sscanf(params, "iI(-1)I(-1)"', VEH)
        for term in ("modelid < 400", "modelid > 611",
                     "color1 < -1", "color1 > 255",
                     "color2 < -1", "color2 > 255",
                     "PLAYER_STATE_ONFOOT", "Race_IsParticipant(playerid)"):
            self.assertIn(term, VEH)

    def test_multiple_cars_and_id_based_deletion(self):
        self.assertIn("new bool:gAdminVehCreated[MAX_VEHICLES + 1]", VEH)
        self.assertIn("gAdminVehCreated[vehicleid] = true;", VEH)
        self.assertIn("cmd:delveh(playerid, params[])", CMDS)
        self.assertIn("return AdminVeh_DeleteCommand(playerid, params);", CMDS)
        self.assertIn('sscanf(params, "s[8]i"', VEH)
        self.assertIn("AdminVeh_DeleteById(playerid, deleteId)", VEH)
        self.assertIn("AdminVeh_IsTracked(vehicleid)", VEH)
        self.assertIn("DestroyVehicle(vehicleid)", VEH)
        self.assertIn("gAdminVehCreated[vehicleid] = false;", VEH)
        self.assertNotIn("gAdminVehVehicle[MAX_PLAYERS]", VEH)
        self.assertNotIn("PutPlayerInVehicle(", VEH)

    def test_antiflood_audit_and_lifecycle(self):
        self.assertIn("#define ADMIN_VEH_COOLDOWN_SEC (3)", VEH)
        self.assertIn("AdminVeh_CooldownReady(playerid)", VEH)
        self.assertIn("gAdminVehNextAction[playerid] = gettime() +", VEH)
        self.assertIn('Admin_LogPlayer(playerid, "VEH_CREATE", detail)', VEH)
        self.assertIn('Admin_LogPlayer(playerid, "VEH_DELETE", detail)', VEH)
        self.assertLess(VEH.index('Admin_LogPlayer(playerid, "VEH_CREATE", detail)'),
                        VEH.index("CreateVehicle(modelid"))
        self.assertIn("AdminVeh_Init();", MAIN)
        self.assertIn("AdminVeh_Reset(playerid);", MAIN)
        self.assertIn("AdminVeh_Shutdown();", MAIN)
        self.assertIn("AdminVeh_OnVehicleDeath(vehicleid);", MAIN)
        self.assertNotIn("AdminVeh_Destroy(playerid);", MAIN)
        self.assertIn("SetVehicleVirtualWorld(vehicleid, world)", VEH)
        self.assertIn("LinkVehicleToInterior(vehicleid, interior)", VEH)
        self.assertNotIn("mysql_", VEH)

    def test_dl_remains_builtin(self):
        self.assertIn("ID через /dl", VEH)
        self.assertNotIn("cmd:dl(", CMDS)
        self.assertIn("/veh del ID", CMDS)


if __name__ == "__main__":
    unittest.main()
