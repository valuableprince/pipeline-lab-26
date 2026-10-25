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

    def test_six_randomized_objects_and_three_russian_categories(self):
        for material in ('"БУМАГА"', '"ПЛАСТИК"', '"МЕТАЛЛ"'):
            self.assertIn(material, UI)
        self.assertIn('#define SORT_MODELS_PER_CATEGORY (10)', UI)
        self.assertIn('#define SORT_CATALOG_COUNT (SORT_CATEGORY_COUNT * SORT_MODELS_PER_CATEGORY)', UI)
        self.assertIn('#define SORT_ITEM_COUNT (6)', UI)
        self.assertIn('#define SORT_CATEGORY_COUNT (3)', UI)
        self.assertIn('TEXT_DRAW_FONT_MODEL_PREVIEW', UI)
        self.assertNotIn('PlayerTextDrawSetPreviewModel(playerid, bag, 1265)', UI)
        self.assertNotIn('new PlayerText:bag', UI)
        self.assertIn('"01 / ПРЕДМЕТЫ"', UI)

    def test_no_object_captions_and_dark_ui(self):
        self.assertIn("#define SORT_TD_TOTAL (22)", UI)
        self.assertNotIn("SORT_TD_LABEL_START", UI)
        self.assertNotIn("gSortItemNames", UI)
        self.assertIn("0x171B21F5", UI)
        self.assertIn("0x242930FA", UI)
        self.assertIn("0x10161DF0", UI)
        self.assertIn("0x131920F0", UI)
        self.assertIn("PlayerTextDrawSetPreviewModel(playerid, preview, gSortCatalogModels[catalog]);", UI)
        self.assertIn("PlayerTextDrawSetSelectable(playerid, preview, true);", UI)
        self.assertIn("gSortCatalogRot[catalog][0]", UI)
        self.assertIn("2437", UI)
        self.assertIn("18633", UI)
        self.assertIn("[Сортировка]", UI)

    def test_sprite_panels_have_actual_pixel_dimensions_and_no_bag(self):
        self.assertIn('CreatePlayerTextDraw(playerid, x, y, "LD_SPAC:white")', UI)
        self.assertIn('PlayerTextDrawFont(playerid, td, TEXT_DRAW_FONT_SPRITE);', UI)
        self.assertIn('PlayerTextDrawTextSize(playerid, td, width, height);', UI)
        self.assertIn('PlayerTextDrawColour(playerid, td, colour);', UI)
        for panel in (
            'SortUI_Rect(playerid, 0, 84.0, 82.0, 472.0, 291.0,',
            'SortUI_Rect(playerid, 1, 90.0, 88.0, 460.0, 56.0,',
            'SortUI_Rect(playerid, 4, 102.0, 153.0, 214.0, 177.0,',
            'SortUI_Rect(playerid, 5, 324.0, 153.0, 214.0, 177.0,',
        ):
            self.assertIn(panel, UI)
        self.assertNotIn('new PlayerText:bag', UI)
        self.assertNotIn('PlayerTextDrawSetPreviewModel(playerid, bag', UI)
        self.assertLess(UI.index('SortUI_Rect(playerid, 5,'),
                        UI.index('for (new item = 0; item < SORT_ITEM_COUNT; item++)',
                                 UI.index('stock SortUI_Open(playerid)')))

    def test_catalog_has_30_distinct_models_ten_per_category(self):
        table = re.search(
            r"new const gSortCatalogModels\[SORT_CATALOG_COUNT\] =\s*\{(.*?)\};",
            UI, re.S)
        self.assertIsNotNone(table)
        rows = re.findall(r"^\s*(\d+),?\s*//\s*(.+)$", table.group(1), re.M)
        ids = [int(model) for model, _ in rows]
        self.assertEqual(len(ids), 30)
        self.assertEqual(len(set(ids)), 30)
        self.assertEqual(ids[:10], [2852, 1220, 1221, 1230, 2059,
                                    2894, 1582, 2663, 19561, 19562])
        self.assertEqual(ids[10:20], [2647, 2767, 19560, 19567, 19568,
                                      19570, 19587, 19592, 1238, 2437])
        self.assertEqual(ids[20:], [2601, 18633, 18634, 18635, 18644,
                                    2057, 19581, 19583, 19584, 19586])
        rotations = re.search(
            r"new const Float:gSortCatalogRot\[SORT_CATALOG_COUNT\]\[4\] =\s*\{(.*?)\};",
            UI, re.S)
        self.assertIsNotNone(rotations)
        self.assertEqual(rotations.group(1).count("//"), 30)

    def test_round_generator_guarantees_coverage_uniqueness_shuffle_and_change(self):
        generator = UI.split("stock SortUI_RollItems(playerid)", 1)[1].split(
            "stock SortUI_Status", 1)[0]
        self.assertIn("random(SORT_MODELS_PER_CATEGORY)", generator)
        self.assertIn("category * SORT_MODELS_PER_CATEGORY", generator)
        self.assertIn("if (!chosen) pool[poolSize++] = catalog;", generator)
        self.assertIn("new pick = random(poolSize);", generator)
        self.assertIn("pool[pick] = pool[poolSize];", generator)
        self.assertIn("new pick = random(slot + 1);", generator)
        self.assertIn("gSortHasLastRound[playerid]", generator)
        self.assertIn("if (unchanged)", generator)
        self.assertIn("if (!alreadyUsed)", generator)
        self.assertIn("gSortLastRound[playerid][slot] = gSortRoundItems[playerid][slot];", generator)
        self.assertIn("SortUI_RollItems(playerid);", UI)
        self.assertIn("gSortHasLastRound[playerid] = false;", UI)
        self.assertIn("gSortRoundItems[playerid][selected]", UI)
        self.assertIn("catalog / SORT_MODELS_PER_CATEGORY != category", UI)
        self.assertNotIn("gSortItemCategory", UI)
        self.assertNotIn("gSortItemModels", UI)

    def test_item_tiles_do_not_expose_material_by_colour(self):
        self.assertIn("PlayerTextDrawBoxColour(playerid, preview, 0x27323BEE);", UI)
        self.assertIn("new box = 0x27323BEE;", UI)
        self.assertIn("if (gSortSelection[playerid] == i) box = 0xD2A044FF;", UI)

    def test_cyrillic_textdraw_labels_using_cp1251_build(self):
        for sample in ("СОРТИРОВКА ОТХОДОВ", "ПЕРЕРАБОТКА",
                       "ВЫБЕРИТЕ ПРЕДМЕТ, ЗАТЕМ КАТЕГОРИЮ",
                       "СНАЧАЛА ВЫБЕРИТЕ ПРЕДМЕТ",
                       "НЕВЕРНАЯ КАТЕГОРИЯ"):
            self.assertIn(sample, UI)
            self.assertTrue(sample.encode("cp1251"))
        for category in ('"БУМАГА"', '"ПЛАСТИК"', '"МЕТАЛЛ"'):
            self.assertIn(category, UI)
        self.assertIn('"02 / КАТЕГОРИИ"', UI)
        self.assertIn('"ВЫБЕРИТЕ КАТЕГОРИЮ"', UI)
        self.assertIn('"ESC - ВЫХОД"', UI)
        self.assertIn('ALT или /sort', UI)
        self.assertIn('Выберите предмет, затем выберите категорию.', UI)
        for old_english in ('"PAPER"', '"PLASTIC"', '"METAL"',
                            '"01 / MATERIALS"', '"02 / DROP ZONE"',
                            '"ESC - EXIT"', 'УКАЖИТЕ КОНТЕЙНЕР',
                            'ПРЕДМЕТ И КОНТЕЙНЕР'):
            self.assertNotIn(old_english, UI)

    def test_click_to_select_and_server_category_validation(self):
        for fragment in (
            "PlayerTextDrawSetSelectable(playerid, td, true)",
            "SelectTextDraw(playerid, SORT_COLOR_GOLD)",
            "gSortSelection[playerid] = item;",
            "catalog / SORT_MODELS_PER_CATEGORY != category",
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

    def test_sixth_correct_item_closes_immediately_and_notifies(self):
        handler = UI.split("stock SortUI_OnClick(playerid, PlayerText:clickedid)", 1)[1]
        complete = handler.split(
            "if (SortUI_Refresh(playerid) == SORT_ITEM_COUNT)", 1)[1].split(
            'else SortUI_Status(playerid, "ВЕРНО!', 1)[0]
        self.assertIn("SendClientMessage(playerid, CLEANER_COLOR, message);", complete)
        self.assertIn("SortUI_Close(playerid);", complete)
        self.assertIn("return 1;", complete)
        self.assertLess(complete.index("SendClientMessage(playerid, CLEANER_COLOR, message);"),
                        complete.index("SortUI_Close(playerid);"))
        self.assertNotIn("SortUI_Status(playerid", complete)
        self.assertIn("gSortOpen[playerid] = false;", UI)
        self.assertIn("CancelSelectTextDraw(playerid)", UI)

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
