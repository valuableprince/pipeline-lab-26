"""Experimental X>3000 ship probe safety and rollback contracts."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SHIP = (ROOT / "gamemodes/includes/ship_studio.inc").read_text(encoding="utf-8")
PORT = (ROOT / "gamemodes/includes/ship_port.inc").read_text(encoding="utf-8")


class OceanProbeContracts(unittest.TestCase):
    def test_default_normal_map_is_unchanged(self):
        normal = SHIP.split("stock bool:Ship_ValidCoord(", 1)[1].split("stock bool:Ship_ValidManualCoord(", 1)[0]
        self.assertIn("x >= -3000.0 && x <= 3000.0", normal)
        self.assertIn("y >= -3000.0 && y <= 3000.0", normal)
        self.assertIn("gShipOceanProbe = false;", SHIP)
        self.assertIn("stock Ship_Init()", SHIP)

    def test_probe_is_explicit_and_bounded_to_cargo(self):
        probe = SHIP.split("stock bool:Ship_ValidManualCoord(", 1)[1].split("stock bool:Ship_ValidModel(", 1)[0]
        self.assertIn("gShipOceanProbe && gShipModel == SHIP_CARGO_ROOT_ID", probe)
        self.assertIn("x > 3000.0 && x <= SHIP_OCEAN_TEST_MAX_X", probe)
        self.assertIn("#define SHIP_OCEAN_TEST_MAX_X (3600.0)", SHIP)
        self.assertIn('!strcmp(action, "oceanprobe", true)', SHIP)
        self.assertIn('!strcmp(arguments, "on", true)', SHIP)
        self.assertIn('!strcmp(arguments, "off", true)', SHIP)
        self.assertIn("!Ship_Ready()", SHIP)
        self.assertIn("gShipModel != SHIP_CARGO_ROOT_ID", SHIP)

    def test_probe_only_manual_move_or_set(self):
        self.assertIn("if (!Ship_ValidManualCoord(tx, ty, z))", SHIP)
        self.assertIn("!Ship_ValidManualCoord(x, y, z)", SHIP)
        save = SHIP.split("stock bool:Ship_Save()", 1)[1].split("stock bool:Ship_Load()", 1)[0]
        mark = SHIP.split("stock bool:Ship_Mark()", 1)[1].split("stock bool:Ship_Save()", 1)[0]
        self.assertIn("if (!Ship_ValidCoord(x, y, z)) return false;", save)
        self.assertIn("if (!Ship_ValidCoord(x, y, z)) return false;", mark)
        self.assertNotIn("Ship_ValidManualCoord", save)
        self.assertIn("if (!Ship_ValidCoord(x, y, z))", PORT)
        self.assertIn("!Ship_ValidCoord(x, y, z)", SHIP)  # existing normal validation

    def test_safety_off_and_return_to_map(self):
        self.assertIn("if (x > 3000.0)", SHIP)
        self.assertIn("ShipEditor_Occupied()", SHIP)
        self.assertIn("Сначала верните корабль в X<=3000", SHIP)
        self.assertIn("Берег/видимость не гарантированы", SHIP)
        self.assertIn("Не записывайте точку выхода за X=3000", SHIP)


if __name__ == "__main__":
    unittest.main()
