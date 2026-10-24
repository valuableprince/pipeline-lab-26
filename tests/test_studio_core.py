"""STUDIO-CORE-01: контрактные регрессии интеграции игрового редактора."""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
STUDIO = (ROOT / "gamemodes/includes/studio_system.inc").read_text(encoding="utf-8")
MAIN = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
BUILD = (ROOT / "tools/build_windows.py").read_text(encoding="utf-8")
CI = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")


class StudioCoreContract(unittest.TestCase):
    def test_compilation_and_lifecycle(self):
        self.assertIn('#include "studio_system.inc"', MAIN)
        for code in ("Studio_Init();", "Studio_Shutdown();",
                     "Studio_OnDisconnect(playerid);",
                     "Studio_OnDialog(playerid, dialogid, response, listitem)"):
            self.assertIn(code, MAIN)
        self.assertIn('.glob("*.inc")', BUILD)
        self.assertIn('"gamemodes/**"', CI)

    def test_commands_and_founder_only(self):
        self.assertIn("cmd:studio(playerid, params[])", COMMANDS)
        self.assertIn("cmd:zoneedit(playerid, params[])", COMMANDS)
        self.assertIn("return Studio_Command(playerid, params);", COMMANDS)
        self.assertIn("return StudioZone_Command(playerid, params);", COMMANDS)
        self.assertIn("Admin_IsFounder(playerid) && gAdminDuty[playerid]", STUDIO)
        for action in ("create", "move", "radius", "show", "hide",
                       "publish", "cancel", "disable", "status"):
            self.assertIn(f'!strcmp(action, "{action}", true)', STUDIO)

    def test_safe_publication_and_world_limits(self):
        for check in ("TARKENSO_ZONE|1", "gStudioZoneInvalid[slot]",
                      "frename(path, backup)", "frename(temp, path)",
                      "fremove(temp)", "gStudioZoneActive[slot] = active",
                      "StudioZone_Valid(", "StudioZone_Poll()",
                      "GetPlayerVirtualWorld(playerid)",
                      "GetPlayerInterior(playerid)",
                      "gStudioZoneInside[playerid][slot]"):
            self.assertIn(check, STUDIO)
        self.assertIn("Admin_LogPlayer(playerid, \"STUDIO_ZONE_PUBLISH\"", STUDIO)
        self.assertIn("Admin_LogPlayer(playerid, \"STUDIO_ZONE_DISABLE\"", STUDIO)
        self.assertNotIn("SetPlayerCheckpoint(", STUDIO)

    def test_preview_is_private_and_cleaned(self):
        self.assertIn("CreatePlayerObject(playerid", STUDIO)
        self.assertIn("DestroyPlayerObject(gStudioZoneOwner", STUDIO)
        self.assertIn("if (gStudioZoneOwner == playerid) StudioZone_Cancel();", STUDIO)
        self.assertIn("gStudioZoneTimer = SetTimer", STUDIO)
        self.assertIn("KillTimer(gStudioZoneTimer)", STUDIO)


if __name__ == "__main__":
    unittest.main()
