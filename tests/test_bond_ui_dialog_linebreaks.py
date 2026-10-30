"""BOND-UI-01: ensure SA-MP dialogs receive newline escapes, not literal '\n'."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
GOV = (ROOT / "gamemodes/includes/bond_governance_ui.inc").read_text("utf-8")
BONDS = (ROOT / "gamemodes/includes/bonds_ui.inc").read_text("utf-8")


class BondDialogNewlineContracts(unittest.TestCase):
    def test_no_double_escaped_newlines_in_bond_dialog_sources(self):
        # Pawn "\n" inserts a line break; "\\n" displays the literal characters.
        for name, source in (("bond_governance_ui.inc", GOV), ("bonds_ui.inc", BONDS)):
            with self.subTest(source=name):
                self.assertNotIn(r"\\n", source)

    def test_treasury_report_displays_buyback_stats_on_separate_lines(self):
        self.assertIn(
            r"\n{E8EDF3}Досрочно выкуплено: %d шт.\n{E8EDF3}Расходы на выкуп: $%d",
            GOV,
        )

    def test_position_actions_remain_three_distinct_list_rows(self):
        self.assertIn(
            r"Получить выплату по погашению\n{E8EDF3}Продать государству досрочно (98%)\n{E8EDF3}Обновить карточку",
            BONDS,
        )

    def test_buyback_confirmation_and_quantity_are_multiline(self):
        self.assertIn(
            r"ДОСРОЧНАЯ ПРОДАЖА ГОСУДАРСТВУ\n\n{E8EDF3}Выпуск",
            BONDS,
        )
        self.assertIn(
            r"ПОДТВЕРЖДЕНИЕ ДОСРОЧНОЙ ПРОДАЖИ\n\n{E8EDF3}Выпуск",
            BONDS,
        )


if __name__ == "__main__":
    unittest.main()
