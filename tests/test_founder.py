"""FOUNDER-01: защита ника Veyran, офлайн-инициализация и серверные права."""

from pathlib import Path
from unittest import TestCase, mock
import importlib.util
import sys
import tempfile
import types

ROOT = Path(__file__).resolve().parents[1]
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
ADMIN = (ROOT / "gamemodes/includes/admin_system.inc").read_text(encoding="utf-8")
UI = (ROOT / "gamemodes/includes/admin_ui.inc").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text(encoding="utf-8")
SCRIPT = ROOT / "tools/bootstrap_founder.py"

spec = importlib.util.spec_from_file_location("bootstrap_founder", SCRIPT)
founder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(founder)


class FounderContracts(TestCase):
    def test_nickname_is_reserved_from_registration(self):
        self.assertIn('#define FOUNDER_NAME "veyran"', ADMIN)
        self.assertIn('result == 0 && !strcmp(gAccountName[playerid], FOUNDER_NAME, true)', GAME)
        self.assertIn('if (create && !strcmp(gAccountName[playerid], FOUNDER_NAME, true))', GAME)

    def test_level_six_needs_auth_name_and_role(self):
        self.assertIn('gAuthState[playerid] == AUTH_READY', ADMIN)
        self.assertIn('Admin_IsFounderName(gAccountName[playerid]) && gAdminLevel[playerid] == 6', ADMIN)
        self.assertIn("case PERM_OWNER: return gAdminLevel[playerid] == 6;", ADMIN)
        self.assertIn('if (!Admin_IsFounder(playerid))', ADMIN)
        self.assertIn('if (level < 0 || level > 6 || Admin_IsFounderName(gAccountName[playerid]))', ADMIN)
        self.assertIn('case PERM_EVENT: return Admin_IsFounder(playerid)', ADMIN)

    def test_founder_is_not_moderation_target(self):
        self.assertIn('Admin_IsFounderName(gAccountName[targetid])', ADMIN)
        self.assertIn('Admin_IsFounderName(gAccountName[target])', ADMIN)
        self.assertIn('if (!Admin_CanPunish(playerid, i)) continue;', UI)
        self.assertIn('if (Admin_IsFounderName(gAccountName[playerid])) return false;', ADMIN)

    def test_founder_command_is_exclusive_and_audited(self):
        self.assertIn('cmd:founder(playerid, params[])', COMMANDS)
        self.assertIn('if (!Admin_IsFounder(playerid))', ADMIN)
        self.assertIn('Admin_Require(playerid, PERM_OWNER)', ADMIN)
        self.assertIn('Admin_LogPlayer(playerid, "FOUNDER_ANNOUNCE", reason)', ADMIN)

    def test_bootstrap_creates_private_account_and_role(self):
        # Substituting bcrypt tests file layout without needing native bcrypt in CI.
        fake_bcrypt = types.SimpleNamespace(
            gensalt=lambda rounds: b"mock_salt",
            hashpw=lambda password, salt: b"$2b$12$" + b"a" * 53,
        )
        with tempfile.TemporaryDirectory() as base, mock.patch.dict(sys.modules, {"bcrypt": fake_bcrypt}):
            scriptfiles = Path(base) / "scriptfiles"
            account, role = founder.bootstrap(scriptfiles, "LongPassphrase#2026")
            data = account.read_text(encoding="ascii").splitlines()
            self.assertEqual(data, [
                "SIGNAL0317|4", "$2b$12$" + "a" * 53,
                "1", "0", "0", "100.00", "0.00",
                "-1", "0", "0", "0", "0", "0", "0", "0"
            ])
            self.assertEqual(role.read_text(encoding="ascii").splitlines(), ["SIGNALADMIN|1", "6", "0"])
            with self.assertRaises(FileExistsError):
                founder.bootstrap(scriptfiles, "LongPassphrase#2026")
            self.assertEqual(account.read_text(encoding="ascii").splitlines(), data)

    def test_bootstrap_rejects_weak_password(self):
        self.assertFalse(founder.valid_password("12345678"))
        self.assertFalse(founder.valid_password("space not allowed"))
        self.assertTrue(founder.valid_password("Strong#Password2026"))
