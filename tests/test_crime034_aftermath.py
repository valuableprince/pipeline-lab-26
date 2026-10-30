"""CRIME-03.4 static contract: physical routes, one-time state and payout."""
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SQL=(ROOT/"db/migrations/044_crime_tender_aftermath.sql").read_text("utf-8")
A=(ROOT/"gamemodes/includes/crime_aftermath.inc").read_text("utf-8")
I=(ROOT/"gamemodes/includes/crime_incidents.inc").read_text("utf-8")
C=(ROOT/"gamemodes/includes/crime_contracts.inc").read_text("utf-8")
UI=(ROOT/"gamemodes/includes/crime_ui.inc").read_text("utf-8")
G=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
CI=(ROOT/".github/workflows/build.yml").read_text("utf-8")


class Aftermath(unittest.TestCase):
    def test_migration_and_fail_closed(self):
        for word in ("CREATE TABLE crime_tender_aftermath","contract_id INT UNSIGNED NOT NULL PRIMARY KEY",
                     "chk_tender_aftermath_state","choice IN (1,2)","state IN (1,2)",
                     "VALUES (44)"):
            self.assertIn(word,SQL)
        self.assertIn("version=44 LIMIT 1",C)
        self.assertIn("044_crime_tender_aftermath.sql",CI)
        self.assertIn("crime034_mysql_uat.py",CI)
        self.assertIn("CrimeAftermath_Init()",C)

    def test_atomic_single_creation_and_completion(self):
        self.assertIn("INSERT INTO crime_tender_aftermath(contract_id,choice)",I)
        self.assertIn("INSERT INTO crime_contract_events",I)
        self.assertIn("stock bool:CrimeAftermath_Complete(",A)
        for word in ("Crime_Exec(\"START TRANSACTION\")","JOIN faction_members m",
                     "i.choice=a.choice","a.state=1","a.finished_at=0",
                     "a.state=2,a.finished_at=UNIX_TIMESTAMP()",
                     "'AFTERMATH_DONE'","Crime_Rollback()",
                     "Crime_Exec(\"COMMIT\")"):
            self.assertIn(word,A)
        settle=C.split("stock Crime_Settle(",1)[1].split("stock Crime_Abort(",1)[0]
        self.assertIn("c.mode_id<>3 OR (EXISTS(",settle)
        self.assertIn("crime_tender_aftermath x WHERE x.contract_id=c.contract_id AND x.state=2",settle)
        self.assertIn("crime_tender_aftermath x WHERE x.contract_id=%d AND x.state=2",settle)
        self.assertIn("THEN 7 ELSE 2 END",settle)
        self.assertIn("THEN 4 ELSE 1 END",settle)
        self.assertIn("INSERT INTO crime_contract_rewards",settle)

    def test_routing_auth_and_no_new_command(self):
        for word in ("CRIME_AFTERMATH_COUNT (6)","CreateActor(","DestroyActor(",
                     "KEY_YES","CRIME_AFTERMATH_RADIUS (3.5)",
                     "IsPlayerInAnyVehicle(playerid)",
                     "GetPlayerVirtualWorld(playerid)!=0",
                     "GetPlayerInterior(playerid)!=0",
                     "gCrimeUIAuthToken[playerid]!=gAuthToken[playerid]",
                     "gCrimeUIAccountId[playerid]!=gDBPrimaryAccountId[playerid]",
                     "CrimeAftermath_Info(", "CrimeAftermath_Route(",
                     "stateCode==2) return 0;"):
            self.assertIn(word,A)
        self.assertIn("CrimeAftermath_Route(playerid, id)",UI)
        self.assertIn("CrimeAftermath_OnKeys(playerid, newkeys, oldkeys)",G)
        self.assertIn("CrimeAftermath_OnDialog(playerid, dialogid, response, listitem)",G)
        self.assertIn("CrimeAftermath_Reset(playerid);",G)
        self.assertIn("CrimeAftermath_Shutdown();",G)
        self.assertIn('#include "crime_aftermath.inc"',G)
        self.assertNotIn("cmd:crimeaftermath",G)


if __name__=="__main__":
    unittest.main()
