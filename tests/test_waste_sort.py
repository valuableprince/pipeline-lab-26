"""SORT-01 native open.mp TextDraw station contract tests."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / "gamemodes/includes/waste_sort.inc").read_text("utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")
COMMANDS = (ROOT / "gamemodes/includes/commands.inc").read_text("utf-8")


class WasteSortingTextDrawTests(unittest.TestCase):
    def test_game_module_wired_in_lifecycle(self):
        self.assertIn('#include "waste_sort.inc"', GAME)
        self.assertLess(GAME.index('#include "cleaner_truck.inc"'),
                        GAME.index('#include "waste_sort.inc"'))
        for call in ("SortUI_Init();", "SortUI_Shutdown();",
                     "SortUI_Reset(playerid);",
                     "SortUI_OnKeyStateChange(playerid, newkeys, oldkeys)",
                     "SortUI_OnClick(playerid, playertextid)",
                     "SortUI_OnCancel(playerid, clickedid)"):
            self.assertIn(call, GAME)
        self.assertIn("public OnPlayerClickPlayerTextDraw(playerid, PlayerText:playertextid)", GAME)
        self.assertIn("public OnPlayerClickTextDraw(playerid, Text:clickedid)", GAME)

    def test_six_objects_and_three_english_bins(self):
        for material in ('"PAPER"', '"PLASTIC"', '"METAL"'):
            self.assertIn(material, UI)
        self.assertIn('new const gSortItemCategory[SORT_ITEM_COUNT] = {0, 0, 1, 1, 2, 2}', UI)
        self.assertIn('#define SORT_ITEM_COUNT (6)', UI)
        self.assertIn('#define SORT_CATEGORY_COUNT (3)', UI)
        self.assertIn('TEXT_DRAW_FONT_MODEL_PREVIEW', UI)
        self.assertIn('PlayerTextDrawSetPreviewModel(playerid, bag, 1265)', UI)

    def test_no_object_captions_and_dark_ui(self):
        self.assertIn("#define SORT_TD_TOTAL (22)", UI)
        self.assertNotIn("SORT_TD_LABEL_START", UI)
        self.assertNotIn("gSortItemNames", UI)
        self.assertIn("0x1A1D22E8", UI)
        self.assertIn("0x20242AEF", UI)
        self.assertIn("0x171B20E8", UI)
        self.assertIn("PlayerTextDrawSetPreviewModel(playerid, preview, gSortItemModels[item]);", UI)
        self.assertIn("PlayerTextDrawSetSelectable(playerid, preview, true);", UI)
        self.assertIn("gSortItemRot[item][0]", UI)
        self.assertIn("2814", UI)
        self.assertIn("18633", UI)
        self.assertIn("[Сортировка]", UI)

    def test_click_to_select_and_server_category_validation(self):
        for fragment in (
            "PlayerTextDrawSetSelectable(playerid, td, true)",
            "SelectTextDraw(playerid, SORT_COLOR_GOLD)",
            "gSortSelection[playerid] = item;",
            "gSortItemCategory[selected] != category",
            "gSortCompleted[playerid] |= (1 << selected)",
            "gSortErrors[playerid]++",
            "SortUI_Refresh(playerid) == SORT_ITEM_COUNT",
            "gSortOpen[playerid] = false;",
            "CancelSelectTextDraw(playerid)",
            "SortUI_Close(playerid, false)",
        ):
            self.assertIn(fragment, UI)
        self.assertIn('clickedid != Text:INVALID_TEXT_DRAW', UI)
        self.assertIn('if (!SortUI_CanUse(playerid))', UI)

    def test_no_unbacked_rewards_or_unlimited_remote_access(self):
        self.assertIn("(!gCleanerDuty[playerid] && !gTruckOnShift[playerid])", UI)
        self.assertIn("GetPlayerState(playerid) != PLAYER_STATE_ONFOOT", UI)
        self.assertIn("GetPlayerVirtualWorld(playerid) != 0", UI)
        self.assertIn("x*x + y*y + z*z <= SORT_STATION_RADIUS_SQ", UI)
        self.assertNotIn("GivePlayerMoney", UI)
        self.assertNotIn("gAccountMoney[playerid] +=", UI)
        self.assertNotIn("mysql_query", UI)
        self.assertIn("cmd:sort(playerid, params[])", COMMANDS)
        self.assertIn("return SortUI_Command(playerid);", COMMANDS)

    def test_textdraws_destroyed_and_esc_safe(self):
        self.assertIn("PlayerTextDrawDestroy(playerid, gSortDraws[playerid][i]);", UI)
        self.assertIn("if (!gSortOpen[playerid]) return 1;", UI)
        self.assertIn("if (cancelSelection && IsPlayerConnected(playerid))", UI)
        self.assertIn("if (!gSortOpen[playerid] || clickedid != Text:INVALID_TEXT_DRAW)", UI)
        self.assertIn("SortUI_Reset(playerid);", GAME)


if __name__ == "__main__":
    unittest.main()
