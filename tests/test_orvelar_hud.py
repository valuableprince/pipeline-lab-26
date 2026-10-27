"""Статические контракты HUD-01 без запуска игрового клиента."""

from pathlib import Path
from build_contracts import assert_staged
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = (ROOT / "gamemodes" / "signal0317.pwn").read_text(encoding="utf-8")
MODULE = (ROOT / "gamemodes" / "includes" / "orvelar_hud.inc").read_text(encoding="utf-8")
COMMANDS = (ROOT / "gamemodes" / "includes" / "commands.inc").read_text(encoding="utf-8")


class OrvelarHudTest(unittest.TestCase):
    def test_module_is_connected_to_lifecycle(self):
        for token in (
            '#include "orvelar_hud.inc"',
            "OrvelarHUD_Init();",
            "OrvelarHUD_OnConnect(playerid);",
            "OrvelarHUD_Show(playerid);",
            "OrvelarHUD_Shutdown();",
        ):
            self.assertIn(token, SCRIPT)

    def test_three_parts_and_toggle(self):
        for token in (
            '"O"', '"/"', '"Tarkenso RPG"',
            "TextDrawCreate(", "TextDrawShowForPlayer(",
            "TextDrawHideForPlayer(", "TextDrawDestroy(",
            "gOrvelarHudVisible[MAX_PLAYERS]", "OrvelarHUD_Toggle",
        ):
            self.assertIn(token, MODULE)
        self.assertIn("cmd:hudlogo(playerid, params[])", COMMANDS)
        assert_staged(self, "orvelar_hud.inc")

    def test_modern_openmp_colour_names_without_deprecated_aliases(self):
        self.assertEqual(MODULE.count("TextDrawColour("), 3)
        self.assertEqual(MODULE.count("TextDrawBackgroundColour("), 3)
        self.assertNotIn("TextDrawColor(", MODULE)
        self.assertNotIn("TextDrawBackgroundColor(", MODULE)

    def test_does_not_require_unshipped_assets_or_client_patches(self):
        self.assertNotIn("AddSimpleModel(", MODULE)
        self.assertNotIn("mdl-", MODULE)
        self.assertNotIn(".png", MODULE.lower().split("stock orvelarhud_init()")[1])


if __name__ == "__main__":
    unittest.main()
