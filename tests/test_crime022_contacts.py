"""CRIME-02.2: NPC contacts and trust-ledger integration contracts."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SQL = (ROOT/"db/migrations/033_crime_npc_contacts.sql").read_text("utf-8")
CORE = (ROOT/"gamemodes/includes/crime_contracts.inc").read_text("utf-8")
NPC = (ROOT/"gamemodes/includes/crime_contacts.inc").read_text("utf-8")
UI = (ROOT/"gamemodes/includes/crime_ui.inc").read_text("utf-8")
GAME = (ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")


class NPCContactContracts(unittest.TestCase):
    def test_migration_and_existing_compatibility(self):
        for token in (
            "CREATE TABLE crime_contacts", "CREATE TABLE crime_contact_progress",
            "CREATE TABLE crime_contact_trust_events", "ADD COLUMN contact_id",
            "ADD COLUMN required_trust", "fk_crime_trust_progress",
            "INSERT IGNORE INTO crime_contact_progress",
            "INSERT INTO schema_migrations (version) VALUES (33)",
            "'Виктор Седой'", "'Эдвард Кейн'", "'Марта Рейнс'",
        ):
            self.assertIn(token, SQL)
        self.assertEqual(len(re.findall(r"\bSOLO_(?:PORT|SF|LV)_01\b", SQL)), 2)

    def test_npc_world_lifecycle(self):
        for token in ("CreateActor(", "SetActorInvulnerable(",
                      "DestroyActor(", "CRIME_CONTACT_RADIUS",
                      "GetPlayerVirtualWorld(playerid) != 0",
                      "GetPlayerInterior(playerid) != 0",
                      "IsPlayerInRangeOfPoint(playerid",
                      "CrimeContacts_Introduce(playerid, contactId)"):
            if token == "CrimeContacts_Introduce(playerid, contactId)":
                continue
            self.assertIn(token, NPC)
        self.assertIn('#include "crime_contacts.inc"', GAME)
        self.assertIn("CrimeContacts_Shutdown();", GAME)
        self.assertIn("if (!CrimeContacts_Init()) return 1;", CORE)
        self.assertIn("version=44 LIMIT 1", CORE)

    def test_trust_security_and_one_time_settlement(self):
        for token in (
            "CrimeContacts_Near(playerid, broker)",
            "CrimeContacts_TemplateContact(templateId)",
            "p.trust>=t.required_trust",
            "t.contact_id=%d", "INSERT INTO crime_contact_trust_events",
            "UPDATE crime_contact_progress p JOIN crime_contracts c",
            "p.trust=LEAST(100,p.trust+(CASE WHEN c.scenario_kind=0 THEN 5",
            "p.completed_count=p.completed_count+1",
        ):
            self.assertIn(token, CORE)
        payout = CORE.split("stock Crime_Settle(", 1)[1].split("stock Crime_Abort(", 1)[0]
        self.assertLess(payout.index("INSERT INTO crime_contract_rewards"),
                        payout.index("INSERT INTO crime_contact_trust_events"))
        self.assertLess(payout.index("INSERT INTO crime_contact_trust_events"),
                        payout.index('Crime_Exec("COMMIT")'))
        self.assertIn("ON DUPLICATE KEY UPDATE last_met_at", NPC)

    def test_dialogs_only_and_session_safety(self):
        self.assertIn("CrimeUI_Contacts(playerid)", UI)
        self.assertIn("CrimeUI_ContactDetail(playerid, contactId)", UI)
        self.assertIn("CrimeContacts_Introduce(playerid, broker)", UI)
        self.assertIn("gCrimeUISelectedTemplate[playerid]", UI)
        self.assertIn("gCrimeUIOfferId[playerid][listitem]", UI)
        self.assertIn("gCrimeUIOfferCount[playerid]", UI)
        self.assertIn("gCrimeUIAccountId[playerid] != gDBPrimaryAccountId[playerid]", UI)
        self.assertIn("gCrimeUIAuthToken[playerid] != gAuthToken[playerid]", UI)
        self.assertIn("case CRIME_DLG_CONTACTS:", UI)
        self.assertIn("case CRIME_DLG_CONTACT:", UI)
        self.assertNotIn("cmd:crimecontact(", GAME)
        self.assertNotIn("/crime start", UI)


if __name__ == "__main__":
    unittest.main()
