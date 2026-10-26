"""Modernized source-level contract; archived predecessor retained under tests/legacy/."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "gamemodes/includes/admin_system.inc").read_text(encoding="utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
REQUIRED = [
    "Admin_IsFounder(",
    "Admin_CanPunish(",
    "Admin_ValidTarget(",
    "Admin_Require(",
    "Admin_Audit(",
    "Admin_ReadRole("
]


class CurrentContract(unittest.TestCase):
    def test_critical_behavior_contracts_exist(self):
        for marker in REQUIRED:
            with self.subTest(marker=marker):
                self.assertIn(marker, SOURCE)

    def test_module_is_wired_into_current_build(self):
        self.assertIn("#include <open.mp>", GAME)
        self.assertIn('#include "admin_system.inc"', GAME)

    def test_no_client_side_password_or_credential_logging(self):
        self.assertNotIn("printf(query", SOURCE)
        self.assertNotIn("print(password", SOURCE)


if __name__ == "__main__":
    unittest.main()
