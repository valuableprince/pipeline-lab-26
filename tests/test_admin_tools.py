"""Contracts for six audited, duty-gated administrator utility commands."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
CMDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")
TOOLS = (ROOT / "gamemodes/includes/admin_tools.inc").read_text("utf-8")
HELP = (ROOT / "gamemodes/includes/admin_help.inc").read_text("utf-8")
ADMIN = (ROOT / "gamemodes/includes/admin_system.inc").read_text("utf-8")
VEH = (ROOT / "gamemodes/includes/admin_vehicle.inc").read_text("utf-8")
WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text("utf-8")


class AdminToolsContracts(unittest.TestCase):
    def test_all_six_commands_registered_through_pawncmd(self):
        aliases = {
            "ahelp": "AdminHelp_Open(playerid, params)",
            "check": "AdminTools_Check(playerid, params)",
            "heal": "AdminTools_ChangeVitals(playerid, params, true)",
            "armour": "AdminTools_ChangeVitals(playerid, params, false)",
            "fixveh": "AdminTools_Vehicle(playerid, params, false)",
            "flip": "AdminTools_Vehicle(playerid, params, true)",
        }
        for name, call in aliases.items():
            self.assertRegex(CMDS, rf"cmd:{name}\(playerid, params\[\]\)\s*\{{\s*return {re.escape(call)};")
        self.assertIn('#include "admin_tools.inc"', MAIN)
        self.assertIn('#include "admin_tools.inc"\n#include "admin_help.inc"\n#include "admin_tools2.inc"\n#include "admin_punish.inc"\n#include "admin_fly.inc"\n#include "ship_editor.inc"\n#include "ship_camera.inc"\n#include "rp_chat.inc"\n#include "commands.inc"', MAIN)
        self.assertIn("'test_admin_tools.py'", WORKFLOW)

    def test_permissions_and_rank_safety(self):
        for name, permission in (
            ("AdminTools_Help", "PERM_REPORT"),
            ("AdminTools_Check", "PERM_REPORT"),
            ("AdminTools_ChangeVitals", "PERM_MODERATE"),
            ("AdminTools_Vehicle", "PERM_MODERATE"),
        ):
            body = TOOLS.split("stock " + name + "(", 1)[1].split("\n}\n", 1)[0]
            self.assertIn(f"Admin_Require(playerid, {permission})", body)
        self.assertIn("target != playerid && !Admin_ValidTarget(playerid, target)", TOOLS)
        self.assertIn("if (Admin_IsFounderName(gAccountName[targetid])) return false;", ADMIN)
        self.assertIn("gAdminLevel[playerid] >= gAdminLevel[targetid]", ADMIN)
        self.assertIn("AdminHelp_Access(playerid, index)", HELP)
        self.assertIn("gAdminLevel[playerid] >= level", HELP)

    def test_validation_races_and_vehicle_isolation(self):
        self.assertIn("AdminTools_ReadTarget(params, target)", TOOLS)
        self.assertIn("Admin_UnsignedNumber(value)", TOOLS)
        self.assertIn("target >= 0 && target < MAX_PLAYERS", TOOLS)
        self.assertIn("amount > 100", TOOLS)
        self.assertIn("GetPlayerState(target) == PLAYER_STATE_SPECTATING", TOOLS)
        self.assertIn("Race_IsParticipant(target)", TOOLS)
        for term in (
            "IsValidVehicle(vehicleid)",
            "vehicleid <= MAX_VEHICLES",
            "GetVehicleVirtualWorld(vehicleid) != GetPlayerVirtualWorld(playerid)",
            "Race_IsParticipant(i)",
            "GetPlayerVehicleID(i) == vehicleid",
            "AdminVeh_CooldownReady(playerid)",
            "gAdminVehNextAction[playerid] = gettime() + ADMIN_VEH_COOLDOWN_SEC",
        ):
            self.assertIn(term, TOOLS)
        self.assertIn("RepairVehicle(vehicleid);", TOOLS)
        self.assertIn("SetVehicleHealth(vehicleid, 1000.0);", TOOLS)
        self.assertIn("SetVehicleZAngle(vehicleid, angle);", TOOLS)
        self.assertIn("SetVehicleVelocity(vehicleid, 0.0, 0.0, 0.0);", TOOLS)
        self.assertIn("#define ADMIN_VEH_COOLDOWN_SEC (3)", VEH)

    def test_audit_precedes_every_mutation_and_saving(self):
        vitals = TOOLS.split("stock AdminTools_ChangeVitals(", 1)[1].split(
            "stock bool:AdminTools_ReadVehicle(", 1)[0]
        self.assertLess(vitals.index("Admin_LogPlayer(playerid,"), vitals.index("SetPlayerHealth(target, 100.0)"))
        self.assertLess(vitals.index("Admin_LogPlayer(playerid,"), vitals.index("SetPlayerArmour(target, float(amount))"))
        self.assertIn("Auth_SaveAccount(target)", vitals)
        self.assertIn("gAccountHealth[target] = priorHealth;", vitals)
        self.assertIn("gAccountArmour[target] = priorArmour;", vitals)
        vehicle = TOOLS.split("stock AdminTools_Vehicle(", 1)[1]
        self.assertLess(vehicle.index("Admin_LogPlayer(playerid,"), vehicle.index("RepairVehicle(vehicleid);"))
        self.assertLess(vehicle.index("Admin_LogPlayer(playerid,"), vehicle.index("SetVehiclePos(vehicleid,"))
        self.assertIn('"VEH_FLIP" : "VEH_FIX"', vehicle)
        self.assertIn('"HEAL" : "ARMOUR"', vitals)
        self.assertIn('Admin_LogPlayer(playerid, "CHECK", detail)', TOOLS)

    def test_existing_systems_not_removed(self):
        self.assertIn("Admin_ApplyEvent(playerid);", MAIN)
        self.assertIn("Story_OnSpawn(playerid);", MAIN)
        self.assertIn("Race_OnSpawn(playerid);", MAIN)
        self.assertIn("AdminVeh_Init();", MAIN)
        self.assertIn("cmd:veh(playerid, params[])", CMDS)
        self.assertIn("cmd:weather(playerid, params[])", CMDS)
        self.assertIn("cmd:mn(playerid, params[])", CMDS)
        self.assertIn('SendRconCommand("hostname Tarkenso RPG");', MAIN)

    def test_cp1251_sources(self):
        for content in (MAIN, CMDS, TOOLS):
            self.assertEqual(content.encode("cp1251").decode("cp1251"), content)


if __name__ == "__main__":
    unittest.main()
