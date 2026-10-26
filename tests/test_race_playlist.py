"""Current live-game contract replacing a retired source-layout regression."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "gamemodes/includes/race_playlist.inc").read_text(encoding="utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text(encoding="utf-8")
REQUIRED = [
    "RacePlaylist_AllowedUrl(",
    "RacePlaylist_ParseLine(",
    "RacePlaylist_Load(",
    "RacePlaylist_NextStage("
]


class CurrentContract(unittest.TestCase):
    def test_critical_gameplay_entrypoints(self):
        for marker in REQUIRED:
            with self.subTest(marker=marker):
                self.assertIn(marker, SOURCE)

    def test_runtime_module_is_loaded(self):
        self.assertIn('#include "race_playlist.inc"', GAME)

    def test_no_unshipped_npc_runtime_dependency(self):
        self.assertNotIn('#include "npc_route_editor.inc"', GAME)
        self.assertNotIn("printf(query", SOURCE)


if __name__ == "__main__":
    unittest.main()
