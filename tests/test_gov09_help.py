"""Static GOV-09 help contracts: one tutorial for /mn and /bondgov.

The build compiles the real Pawn source separately; these tests catch broken
catalog ordering, lesson navigation and loss of financial caveats.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
EDU = (ROOT / "gamemodes/includes/bond_education.inc").read_text("utf-8")
COMMANDS = (ROOT / "gamemodes/includes/player_command_help.inc").read_text("utf-8")
MENU = (ROOT / "gamemodes/includes/player_menu.inc").read_text("utf-8")
MINFIN = (ROOT / "gamemodes/includes/bond_governance_ui.inc").read_text("utf-8")


class Gov09HelpContracts(unittest.TestCase):
    def test_catalog_has_all_twelve_topics_in_matching_order(self):
        self.assertIn("#define BOND_EDU_TOPIC_COUNT (12)", EDU)
        catalog = EDU.split('"{FFD15C}TARKENSO | Обучение облигациям",', 1)[1]
        catalog = catalog.split('"Читать","Назад"', 1)[0]
        items = re.findall(r"(\d+)\. ([^\n\\]+)", catalog)
        self.assertEqual(len(items), 12, items)
        self.assertEqual([int(i) for i, _ in items], list(range(1, 13)))
        self.assertIn("Досрочная продажа: выкуп за 98%", items[6][1])
        lesson_indices = [int(i) for i in re.findall(
            r"case (\d+): format\(body,sizeof\(body\)", EDU
        )]
        self.assertEqual(lesson_indices, list(range(12)))

    def test_step_by_step_buyback_and_no_double_redemption(self):
        lesson = EDU.split("case 6: format(body,sizeof(body)", 1)[1].split(
            "case 7: format(body,sizeof(body)", 1
        )[0]
        for phrase in (
            "ДОСРОЧНАЯ ПРОДАЖА (GOV-09)",
            "/bonds", "Мой инвестиционный портфель", "Действия",
            "Продать государству досрочно (98%%)",
            "КАК ПРОДАТЬ", "КУПОНЫ", "КОГДА НЕЛЬЗЯ ПРОДАТЬ",
            "2 бумаги по $1000", "$1960", "дисконт $40",
            "Новые купоны по проданным бумагам не начисляются",
            "лимит кошелька", "лимит разовой операции",
            "номинал второй раз не вернут", "Продажа не гарантирована",
        ):
            with self.subTest(phrase=phrase):
                self.assertIn(phrase, lesson)
        self.assertRegex(lesson, r"\{FFD15C\}1\..*\{FFD15C\}5\.")
        self.assertLess(len(lesson), 1700)

    def test_minfin_and_player_share_one_tutorial_and_exit_flow(self):
        self.assertIn("return BondEducation_Open(playerid,0);", MENU)
        self.assertIn("return BondEducation_Open(playerid,1);", MINFIN)
        self.assertIn("if(gBondEduSource[playerid])return GovBond_UI_Main(playerid);", EDU)
        self.assertIn("return PlayerMenu_GameHelp(playerid);", EDU)
        self.assertIn("topic+1,BOND_EDU_TOPIC_COUNT", EDU)
        self.assertIn("return BondEducation_Topic(playerid,gBondEduTopic[playerid]+1);", EDU)
        self.assertIn("К выплате $0", EDU)

    def test_help_mentions_buyback_in_both_command_and_game_menu(self):
        self.assertIn('"bonds", "/bonds", "Покупка и продажа за 98%, портфель, купоны"', COMMANDS)
        self.assertIn('"bondgov", "/bondgov", "Выпуски, подписи, фонд и отчёт о выкупе"', COMMANDS)
        self.assertIn("Помощь по игре -> Облигации -> урок 7", COMMANDS)
        self.assertIn("Облигации: покупка, продажа и купоны", MENU)
        self.assertIn("Досрочно выкуплено", MINFIN)
        self.assertIn("число выкупленных бумаг", EDU)
        self.assertIn("НЕ МОГУ ПРОДАТЬ", EDU)
        self.assertIn("ПОСЛЕ ПРОДАЖИ ПРОПАЛ КУПОН?", EDU)


if __name__ == "__main__":
    unittest.main()
