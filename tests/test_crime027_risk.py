"""CRIME-02.7: contact-local heat economy and fail-closed risk gates."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
SCHEMA=(ROOT/"db/migrations/038_crime_npc_heat.sql").read_text("utf-8")
CORE=(ROOT/"gamemodes/includes/crime_contracts.inc").read_text("utf-8")
UI=(ROOT/"gamemodes/includes/crime_ui.inc").read_text("utf-8")
CONTACT=(ROOT/"gamemodes/includes/crime_contacts.inc").read_text("utf-8")
WORKFLOW=(ROOT/".github/workflows/build.yml").read_text("utf-8")


class SoloRiskEconomy(unittest.TestCase):
    def test_schema_and_ledger(self):
        for token in ("ADD COLUMN heat", "ADD COLUMN risk_delta",
                      "CREATE TABLE crime_contact_heat_events",
                      "contract_id INT UNSIGNED NOT NULL PRIMARY KEY",
                      "fk_crime_heat_progress", "chk_crime_contact_heat",
                      "chk_crime_contract_risk", "VALUES (38)"):
            self.assertIn(token,SCHEMA)
        self.assertIn("version=40 LIMIT 1",CORE)
        self.assertIn("038_crime_npc_heat.sql",WORKFLOW)
        self.assertIn("crime027_mysql_uat.py",WORKFLOW)

    def test_server_side_risk_and_solo_recovery(self):
        for token in ("p.heat<70", "t.scenario_kind=0 OR p.heat<70",
                      "t.cooldown_seconds+p.heat*10",
                      "cooldown += heat * 10",
                      "remaining = -1", "kind > 0 && heat >= 70",
                      "WHEN scenario_kind=0 THEN -8",
                      "WHEN interview_question=2 THEN 8",
                      "WHEN scenario_choice=2 THEN 5",
                      "ELSE -12 END",
                      "crime_contact_heat_events",
                      "risk_delta=CASE"):
            self.assertIn(token,CORE)
        self.assertIn("INSERT INTO crime_contact_heat_events",CORE)
        self.assertIn("CAST(p.heat AS SIGNED)+h.heat_delta",CORE)
        self.assertIn("NOT EXISTS(SELECT 1 FROM crime_contract_rewards",CORE)

    def test_atomic_money_trust_heat_settlement(self):
        block=CORE.split("stock Crime_Settle(",1)[1].split("stock Crime_Abort(",1)[0]
        for token in ('Crime_Exec("START TRANSACTION")', "INSERT INTO crime_contract_rewards",
                      "INSERT INTO crime_contact_trust_events",
                      "INSERT INTO crime_contact_heat_events",
                      "UPDATE crime_contact_progress p JOIN crime_contact_heat_events",
                      'Crime_Exec("COMMIT")', "Crime_Rollback()"):
            self.assertIn(token,block)
        self.assertLess(block.index("INSERT INTO crime_contract_rewards"),
                        block.index("INSERT INTO crime_contact_heat_events"))
        self.assertLess(block.index("INSERT INTO crime_contact_heat_events"),
                        block.index('Crime_Exec("COMMIT")'))
        self.assertIn("GREATEST(-CAST(p.heat AS SIGNED)",block)

    def test_dialogs_explain_heat_and_use_actual_ledgers(self):
        for token in ("CrimeContacts_Heat(playerid, contactId, heat)",
                      "ВНИМАНИЕ", "Высокое внимание",
                      "remaining < 0", "remaining != 0",
                      "COALESCE(h.heat_delta,0) heat_delta",
                      "risk_delta", "риск %d"):
            if token=="risk_delta":
                continue
            self.assertIn(token,UI)
        self.assertIn("stock bool:CrimeContacts_Heat(", CONTACT)
        self.assertIn("gDBPrimaryAccountId[playerid]", CONTACT)
        self.assertNotIn("cmd:crimeheat(",UI+CONTACT)


if __name__=="__main__":
    unittest.main()
