"""Contract for disabling stock GTA San Andreas stunt bonuses."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "gamemodes" / "signal0317.pwn").read_text("utf-8")

class StuntBonusContract(unittest.TestCase):
    def test_global_stunt_rewards_are_disabled(self):
        self.assertIn("EnableStuntBonusForAll(false);", SOURCE)

    def test_bonus_is_disabled_for_each_connecting_player(self):
        start = SOURCE.index("public OnPlayerConnect(playerid)")
        end = SOURCE.index("public OnPlayerDisconnect", start)
        self.assertIn("EnableStuntBonusForPlayer(playerid, false);", SOURCE[start:end])

    def test_no_reenable(self):
        self.assertNotIn("EnableStuntBonusForAll(true)", SOURCE)
        self.assertNotIn("EnableStuntBonusForPlayer(playerid, true)", SOURCE)

if __name__ == "__main__":
    unittest.main()
