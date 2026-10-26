"""Current live-game contract replacing a retired source-layout regression."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "gamemodes/includes/studio_system.inc").read_text(encoding="utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
REQUIRED = [
    "Studio_Allowed(",
    "Studio_Open(",
    "Studio_Command(",
    "Studio_OnDialog(",
    "StudioZone_Command(",
    "Studio_Shutdown("
]


class CurrentContract(unittest.TestCase):
    def test_critical_gameplay_entrypoints(self):
        for marker in REQUIRED:
            with self.subTest(marker=marker):
                self.assertIn(marker, SOURCE)

    def test_runtime_module_is_loaded(self):
        self.assertIn('#include "studio_system.inc"', GAME)

    def test_no_unshipped_npc_runtime_dependency(self):
        self.assertNotIn('#include "npc_route_editor.inc"', GAME)
        self.assertFalse((ROOT / "gamemodes/includes/npc_route_editor.inc").exists())


if __name__ == "__main__":
    unittest.main()
