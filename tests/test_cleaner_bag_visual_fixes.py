"""Regression checks for cleaner bag visuals and full-container notifications."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
CONTAINERS = (ROOT / "gamemodes/includes/cleaner_container.inc").read_text("utf-8")
CLEANER = (ROOT / "gamemodes/includes/cleaner_job.inc").read_text("utf-8")

class CleanerBagVisualFixes(unittest.TestCase):
    def test_hand_bag_uses_second_gta_model(self):
        self.assertIn("#define CLEANER_BAG_MODEL       (1265)", CLEANER)
        self.assertIn("SetPlayerAttachedObject(playerid, slot, CLEANER_BAG_MODEL, 6,", CLEANER)

    def test_no_container_spam_is_bounded(self):
        self.assertIn("CLEANER_DUMP_NO_SPACE_COOLDOWN (60)", CONTAINERS)
        self.assertIn("if (now >= gCleanerDumpNoSpaceNotice[playerid])", CONTAINERS)
        self.assertIn("gCleanerDumpNoSpaceNotice[playerid] = now + CLEANER_DUMP_NO_SPACE_COOLDOWN;", CONTAINERS)
        self.assertIn("gCleanerDumpNoSpaceNotice[playerid] = 0;", CONTAINERS)

    def test_bag_placement_uses_mapped_coordinates_with_vertical_adjustment(self):
        self.assertIn("gCleanerDumpBagPos[slot][j][2] - CLEANER_DUMP_GROUND_OFFSET", CONTAINERS)
        self.assertIn("CLEANER_DUMP_GROUND_OFFSET (0.35)", CONTAINERS)

if __name__ == "__main__":
    unittest.main()
