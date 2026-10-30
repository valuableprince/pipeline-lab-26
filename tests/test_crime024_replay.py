"""CRIME-02.4: stable circumstances, independent cooldowns, one-time guard."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SQL = (ROOT / "db/migrations/035_crime_replay_variants.sql").read_text("utf-8")
CORE = (ROOT / "gamemodes/includes/crime_contracts.inc").read_text("utf-8")
UI = (ROOT / "gamemodes/includes/crime_ui.inc").read_text("utf-8")
SCENES = (ROOT / "gamemodes/includes/crime_scenarios.inc").read_text("utf-8")
GAME = (ROOT / "gamemodes/signal0317.pwn").read_text("utf-8")


class Crime04ReplayContracts(unittest.TestCase):
    def test_schema_and_backward_compatibility(self):
        for token in (
            "ADD COLUMN cooldown_seconds", "ADD COLUMN is_repeatable",
            "ADD COLUMN scenario_event", "idx_crime_repeat_history",
            "UPDATE crime_contracts", "WHERE scenario_kind BETWEEN 1 AND 3",
            "WHERE template_id IN (1,2,3)", "SET cooldown_seconds=300",
            "WHERE template_id IN (4,5,6)", "SET cooldown_seconds=900",
            "chk_crime_event_variant", "VALUES (35)",
        ):
            self.assertIn(token, SQL)
        self.assertNotIn("DELETE FROM", SQL)
        self.assertIn("version=41 LIMIT 1", CORE)

    def test_authoritative_acceptance_and_one_active(self):
        for token in (
            "Crime_OfferEligibility(playerid, templateId, left, finished)",
            "t.is_repeatable=1 OR NOT EXISTS",
            "h.state IN(3,5)", "h.state=3",
            "h.finished_at>UNIX_TIMESTAMP()-(t.cooldown_seconds+p.heat*10)",
            "CASE WHEN t.scenario_kind=0 THEN 0 ELSE 1+FLOOR(RAND()*3) END",
            "INSERT INTO crime_contracts",
            "Crime_Rollback()",
        ):
            self.assertIn(token, CORE)
        self.assertIn("uq_crime_one_active_contract",
                      (ROOT/"db/migrations/032_crime_contract_core.sql").read_text("utf-8"))

    def test_dialog_cooldown_and_case_variation(self):
        for token in (
            "Crime_OfferEligibility(playerid, offerIds[i]",
            "new offerIds[CRIME_OFFER_MAX]",
            "cache_delete(c);",
            "if (alreadyDone)", "Через %d мин", "Пройдено",
            "gCrimeUISelectedTemplate[playerid] = alreadyDone || remaining != 0 ? 0 : templateId",
            "CrimeScenario_EventInfo(playerid, id, eventId)",
            "CrimeScenario_EventText(kind, eventId, phase",
        ):
            self.assertIn(token, UI)
        self.assertIn("stock bool:CrimeScenario_EventInfo", SCENES)
        self.assertIn("SELECT scenario_event FROM crime_contracts", SCENES)
        self.assertIn("case 1:", SCENES)
        self.assertIn("case 2:", SCENES)
        self.assertIn("case 3:", SCENES)
        self.assertIn('#include "crime_scenarios.inc"', GAME)
        self.assertNotIn("cmd:crimerepeat", GAME)


if __name__ == "__main__":
    unittest.main()
