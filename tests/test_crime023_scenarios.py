"""CRIME-02.3: distinct solo case scripts and persistent decisions."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
SQL=(ROOT/"db/migrations/034_crime_solo_scenarios.sql").read_text("utf-8")
CORE=(ROOT/"gamemodes/includes/crime_contracts.inc").read_text("utf-8")
UI=(ROOT/"gamemodes/includes/crime_ui.inc").read_text("utf-8")
SCRIPTS=(ROOT/"gamemodes/includes/crime_scenarios.inc").read_text("utf-8")
GAME=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")


class SoloScenarios(unittest.TestCase):
    def test_schema_and_existing_couriers(self):
        for t in ("ADD COLUMN scenario_kind", "ADD COLUMN scenario_choice",
                  "ADD COLUMN scenario_resolution", "BETWEEN 0 AND 3",
                  "BETWEEN 0 AND 2", "VALUES (34)",
                  "'Пропавший курьер'", "'Подозрительный склад'",
                  "'Сорванная сделка'"):
            self.assertIn(t, SQL)
        for template, kind in (("SOLO_SF_MISSING", 1),
                               ("SOLO_LV_WAREHOUSE", 2),
                               ("SOLO_LS_DISPUTE", 3)):
            self.assertIn(template, SQL)
            self.assertIn(f",5,{kind})", SQL)
        self.assertIn("NOT NULL DEFAULT 0", SQL)
        self.assertIn("version=34 LIMIT 1", CORE)

    def test_persistent_stages_and_atomic_outcomes(self):
        for token in ("t.contact_id,t.scenario_kind",
                      "scenario_choice=%d", "CLUE_FOUND",
                      "scenario_resolution=%d",
                      "c.scenario_choice IN(1,2)",
                      "INSERT INTO crime_contact_trust_events",
                      "WHEN scenario_resolution=2 THEN 2",
                      "WHEN scenario_choice=1 THEN 5 ELSE 3",
                      "LEAST(100", "Crime_Rollback();"):
            self.assertIn(token, CORE)
        fragment=CORE.split("stock Crime_Settle(",1)[1].split("stock Crime_Abort(",1)[0]
        self.assertLess(fragment.index("scenario_resolution=%d"),
                        fragment.index("INSERT INTO crime_contact_trust_events"))
        self.assertLess(fragment.index("INSERT INTO crime_contact_trust_events"),
                        fragment.index('Crime_Exec("COMMIT")'))

    def test_player_dialogs_and_all_three_narratives(self):
        self.assertIn('#include "crime_scenarios.inc"', GAME)
        self.assertIn("#define CRIME_DLG_SCENE      (7961)", UI)
        self.assertIn("case CRIME_DLG_SCENE:", UI)
        self.assertIn("gCrimeUISceneContract[playerid]", UI)
        self.assertIn("gCrimeUIScenePhase[playerid]", UI)
        self.assertIn("CrimeScenario_Info(playerid, id, kind, choice)", UI)
        self.assertIn("Crime_Advance(playerid, phase == 2, listitem + 1)", UI)
        self.assertIn("gCrimeUIAuthToken[playerid] != gAuthToken[playerid]", UI)
        for choice in ("Изучить маршрутный журнал", "Опросить случайного свидетеля",
                       "Сверить складские записи", "Осмотреть только пломбы",
                       "Проверить расписку", "Выслушать одну сторону",
                       "Передать полное досье", "Умалчивать о находке",
                       "Предложить честное примирение"):
            self.assertIn(choice, SCRIPTS)
        self.assertIn("IsPlayerInRangeOfPoint(playerid, CRIME_INTERACT_RADIUS", UI)
        self.assertNotIn("cmd:crimescene", GAME)


if __name__ == "__main__":
    unittest.main()
