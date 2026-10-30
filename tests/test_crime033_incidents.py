"""Static regression: CRIME-03.3 cannot be bypassed from old dialogs."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DB=(ROOT/"db/migrations/043_crime_tender_incidents.sql").read_text("utf-8")
CORE=(ROOT/"gamemodes/includes/crime_contracts.inc").read_text("utf-8")
INC=(ROOT/"gamemodes/includes/crime_incidents.inc").read_text("utf-8")
TENDER=(ROOT/"gamemodes/includes/crime_tenders.inc").read_text("utf-8")
UI=(ROOT/"gamemodes/includes/crime_ui.inc").read_text("utf-8")
GAME=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
BUILD=(ROOT/".github/workflows/build.yml").read_text("utf-8")

class IncidentContracts(unittest.TestCase):
    def test_schema_migration_and_backfill(self):
        for token in ("CREATE TABLE crime_tender_incidents","contract_id INT UNSIGNED NOT NULL PRIMARY KEY",
                      "incident_code","resolved_at","mode_id=3 AND state IN(1,2)",
                      "VALUES (43)"):
            self.assertIn(token,DB)
        self.assertIn("version=43 LIMIT 1",CORE)
        self.assertIn("043_crime_tender_incidents.sql",BUILD)
        self.assertIn("crime033_mysql_uat.py",BUILD)

    def test_transactional_award_and_wallet_gate(self):
        self.assertIn("INSERT INTO crime_tender_incidents(contract_id,incident_code)",TENDER)
        section=TENDER.split("stock bool:CrimeTender_Finalize(",1)[1].split("stock bool:CrimeTender_Refresh(",1)[0]
        self.assertLess(section.index('START TRANSACTION'),section.index('INSERT INTO crime_contracts'))
        self.assertLess(section.index('INSERT INTO crime_contracts'),section.index('INSERT INTO crime_tender_incidents'))
        self.assertLess(section.index('INSERT INTO crime_tender_incidents'),section.index('Crime_Exec("COMMIT")'))
        settle=CORE.split("stock Crime_Settle(",1)[1].split("stock Crime_Abort(",1)[0]
        self.assertIn("c.mode_id<>3 OR EXISTS(SELECT 1 FROM crime_tender_incidents",settle)
        self.assertIn("mode_id<>3 OR EXISTS(SELECT 1 FROM crime_tender_incidents",settle)
        self.assertIn("risk_delta=CASE WHEN mode_id=3",settle)
        self.assertIn("WHEN mode_id=3 THEN -12",settle)
        self.assertIn("THEN 12",settle)
        self.assertIn("reward_amount+300",INC)

    def test_one_time_sanitized_world_interaction(self):
        for token in ("stock bool:CrimeIncident_Init","CreateActor(",
                      "DestroyActor(","KEY_YES","IsPlayerInAnyVehicle",
                      "GetPlayerVirtualWorld(playerid)!=0","GetPlayerInterior(playerid)!=0",
                      "CRIME_INCIDENT_RADIUS (3.5)","CrimeIncident_Info(",
                      "CrimeUI_Open(playerid,CRIME_INCIDENT_DLG)",
                      "gCrimeUIAuthToken[playerid]!=gAuthToken[playerid]",
                      "gCrimeUIAccountId[playerid]!=gDBPrimaryAccountId[playerid]",
                      "JOIN faction_members m","i.choice=0",
                      "state=2","expires_at>UNIX_TIMESTAMP()",
                      "INCIDENT_RESOLVED", "Crime_Rollback()"):
            self.assertIn(token,INC)
        self.assertIn("CrimeIncident_Route(playerid, id)",UI)
        self.assertIn("CrimeIncident_OnKeys(playerid, newkeys, oldkeys)",GAME)
        self.assertIn("CrimeIncident_OnDialog(playerid, dialogid, response, listitem)",GAME)
        self.assertIn("CrimeIncident_Shutdown();",GAME)
        self.assertIn('#include "crime_incidents.inc"',GAME)

if __name__=="__main__":
    unittest.main()
