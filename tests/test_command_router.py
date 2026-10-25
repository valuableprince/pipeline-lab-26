"""Проверки нового процессора команд Pawn.CMD + sscanf без запуска сервера."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
ADMIN = (ROOT / "gamemodes/includes/admin_system.inc").read_text(encoding="utf-8")
DOC = (ROOT / "docs/COMMAND_PROCESSOR.md").read_text(encoding="utf-8")
BUILD = (ROOT / "tools/build_windows.py").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/pawn-build.yml").read_text(encoding="utf-8")


class PawnCmdMigrationTests(unittest.TestCase):
    def test_plugin_router_replaces_legacy_dispatcher(self):
        self.assertNotIn("OnPlayerCommandText", GAME)
        self.assertIn('#include "Pawn.CMD.inc"', GAME)
        self.assertIn('#include "sscanf2.inc"', GAME)
        self.assertIn('#include "commands.inc"', GAME)
        self.assertIn("public OnPlayerCommandReceived(", COMMANDS)
        self.assertIn("gAuthState[playerid] != AUTH_READY", COMMANDS)
        self.assertIn("public OnPlayerCommandPerformed(", COMMANDS)

    def test_all_existing_commands_registered_once(self):
        names = re.findall(r"^cmd:([a-z]+)\(playerid, params\[\]\)", COMMANDS, re.MULTILINE)
        expected = set(("help story position hudlogo report admins admin aduty a reports "
                        "accept areply close spec warn mute kick freeze goto gethere "
                        "ban unban setadmin setevent event weather alog founder").split())
        expected.update(("leaders fmenu leader fstats fchat finvite faccept "
                         "funinvite frank fleave setleader unleader renewleader").split())
        expected.update(("trashdrop trashservice cleanstats orders truckpark truckcancel truckveh guardstatus npcroute studio zoneedit nset tpcor").split())
        self.assertEqual(set(names), expected)
        self.assertEqual(len(names), len(set(names)))

    def test_auth_gate_and_admin_permissions_preserved(self):
        self.assertIn("return Admin_Execute(playerid, input);", COMMANDS)
        self.assertIn("Admin_Require(playerid, PERM_MODERATE)", ADMIN)
        self.assertIn("moderation ? PERM_MODERATE : PERM_BAN_DAY", ADMIN)
        self.assertIn("Admin_Require(playerid, PERM_MANAGE_STAFF)", ADMIN)
        self.assertIn("gAdminLevel[target] >= gAdminLevel[playerid]", ADMIN)

    def test_sscanf_covers_numeric_sensitive_routes(self):
        for name in ("setadmin", "setevent", "ban", "mute"):
            self.assertRegex(COMMANDS, rf"cmd:{name}\(playerid, params\[\]\)\s*\{{\s*new .*?;\s*if \(sscanf\(params, \"ii\",")
        self.assertIn("#define SSCANF_NO_NICE_FEATURES", GAME)

    def test_windows_and_ci_stage_all_includes(self):
        for name in ("Pawn.CMD.inc", "sscanf2.inc", "commands.inc"):
            self.assertIn('"' + name + '"', BUILD)
            self.assertIn('"' + name + '"', WORKFLOW)
            self.assertTrue((ROOT / "gamemodes/includes" / name).is_file())
        self.assertIn("branches: [main]", WORKFLOW)

    def test_versions_and_uat_documented(self):
        for token in ("3.4.0-omp", "v2.13.8", "components",
                      "samp_bcrypt", "Ручная приёмка", "откат"):
            self.assertIn(token.lower(), DOC.lower())


if __name__ == "__main__":
    unittest.main()
