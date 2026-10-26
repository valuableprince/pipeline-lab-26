"""Current implementation contract, replacing an obsolete historical assertion set."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "gamemodes/includes/db_shadow.inc").read_text(encoding="utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
REQUIRED = [
    "DBShadow_CheckAccount(",
    "DBShadow_OnAccountResult(",
    "mysql_tquery(",
    "DBShadow_OnDisconnect("
]


class CurrentContract(unittest.TestCase):
    def test_current_server_contracts_exist(self):
        for marker in REQUIRED:
            with self.subTest(marker=marker):
                self.assertIn(marker, SOURCE)

    def test_current_module_is_integrated(self):
        self.assertIn("#include <open.mp>", GAME)
        self.assertIn('#include "db_shadow.inc"', GAME)

    def test_input_is_not_used_for_raw_debug_logging(self):
        self.assertNotIn("printf(query", SOURCE)
        self.assertNotIn("print(password", SOURCE)


if __name__ == "__main__":
    unittest.main()
