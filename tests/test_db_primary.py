"""Current implementation contract, replacing an obsolete historical assertion set."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "gamemodes/includes/db_primary.inc").read_text(encoding="utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
REQUIRED = [
    "DBPrimary_BeginAccount(",
    "DBPrimary_WriteAccount(",
    "DBPrimary_Execute(",
    "DBPrimary_RecoveryTick(",
    "DBPrimary_AccountTimeout(",
    "row_version=row_version+1"
]


class CurrentContract(unittest.TestCase):
    def test_current_server_contracts_exist(self):
        for marker in REQUIRED:
            with self.subTest(marker=marker):
                self.assertIn(marker, SOURCE)

    def test_current_module_is_integrated(self):
        self.assertIn("#include <open.mp>", GAME)
        self.assertIn('#include "db_primary.inc"', GAME)

    def test_input_is_not_used_for_raw_debug_logging(self):
        self.assertNotIn("printf(query", SOURCE)
        self.assertNotIn("print(password", SOURCE)


if __name__ == "__main__":
    unittest.main()
