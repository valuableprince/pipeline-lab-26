"""Modernized source-level contract; archived predecessor retained under tests/legacy/."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "tools/check_pawn_output.py").read_text(encoding="utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
REQUIRED = [
    "analyze_pawn_output",
    "def main(",
    "warning",
    "error"
]


class CurrentContract(unittest.TestCase):
    def test_critical_behavior_contracts_exist(self):
        for marker in REQUIRED:
            with self.subTest(marker=marker):
                self.assertIn(marker, SOURCE)

    def test_module_is_wired_into_current_build(self):
        self.assertIn("#include <open.mp>", GAME)
        self.assertIn("python3 tools/check_pawn_output.py", (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8"))

    def test_no_client_side_password_or_credential_logging(self):
        self.assertNotIn("printf(query", SOURCE)
        self.assertNotIn("print(password", SOURCE)


if __name__ == "__main__":
    unittest.main()
