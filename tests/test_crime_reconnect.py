"""Reconnect notice contract: one login trigger, SQL ownership and no new money."""
import re
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
MAIN=(ROOT/"gamemodes/signal0317.pwn").read_text(encoding="utf-8")
RECONNECT=(ROOT/"gamemodes/includes/crime_reconnect.inc").read_text(encoding="utf-8")
CORE=(ROOT/"gamemodes/includes/crime_contracts.inc").read_text(encoding="utf-8")
TENDER=(ROOT/"gamemodes/includes/crime_tenders.inc").read_text(encoding="utf-8")
WORKFLOW=(ROOT/".github/workflows/build.yml").read_text(encoding="utf-8")


class ReconnectNotice(unittest.TestCase):
    def test_login_only_after_auth_and_spawn(self):
        self.assertIn('#include "crime_reconnect.inc"',MAIN)
        section=MAIN.split("stock Auth_EnterWorld(playerid)",1)[1].split("stock Auth_FindLockSlot",1)[0]
        self.assertEqual(section.count("CrimeReconnect_Notify(playerid);"),1)
        self.assertLess(section.index("gAuthState[playerid] = AUTH_READY;"),
                        section.index("CrimeReconnect_Notify(playerid);"))
        self.assertLess(section.index("Leader_OnReady(playerid);"),
                        section.index("CrimeReconnect_Notify(playerid);"))
        self.assertLess(section.index("SpawnPlayer(playerid);"),
                        section.index("CrimeReconnect_Notify(playerid);"))
        self.assertNotIn("CrimeReconnect_Notify(playerid);",
                         MAIN.split("public OnPlayerSpawn(playerid)",1)[1])
        self.assertGreaterEqual(MAIN.count("CrimeReconnect_Reset(playerid);"),2)
        self.assertIn("gCrimeReconnectNotified[playerid]=true;",RECONNECT)
        self.assertIn("gCrimeReconnectNotified[playerid]) return 1;",RECONNECT)
        self.assertNotIn("SetTimer(",RECONNECT)

    def test_durable_owned_state_and_real_deadline(self):
        for required in [
            "Crime_Available(playerid)",
            "Leader_IsCriminal(Leader_PlayerOrg(playerid))",
            "gDBPrimaryAccountId[playerid]",
            "JOIN crime_contract_templates t ON t.template_id=c.template_id",
            "c.state IN(1,2) LIMIT 2",
            "cache_get_value_name_int(0,\"expires_at\",deadline)",
            "deadline<=gettime()",
            "Crime_Expire(playerid)",
            "if(!Crime_Expire(playerid))",
            "CrimeScenario_StageLabel(kind,phase,stage,sizeof(stage))",
            "CrimeIncident_Info(playerid,contractId,contact,code,choice)",
            "CrimeAftermath_Info(playerid,contractId,",
            "Встреча с первым связным",
            "Встреча со вторым связным",
            "deadline-gettime()+59",
            "SendClientMessage(playerid,CRIME_COLOR,text)",
            "Продолжить задание: /crime",
            "Контракт #%d",
            "Не удалось проверить незавершённые задания",
        ]:
            self.assertIn(required,RECONNECT)
        self.assertIn("'EXPIRED'",CORE)
        self.assertIn("INSERT IGNORE INTO crime_contract_events",CORE)
        self.assertIn("CrimeTender_PenalizeExpired(playerid)",CORE)
        self.assertIn('Crime_Exec("COMMIT")',CORE)
        # This is just a notification, not a second mission or a payout.
        for unsafe in [
            "INSERT INTO crime_contracts",
            "INSERT INTO crime_contract_rewards",
            "UPDATE accounts",
            "CREATE TABLE",
            "Crime_Start(",
            "Crime_Settle(",
            "SetPlayerCheckpoint(",
        ]:
            self.assertNotIn(unsafe,RECONNECT)

    def test_tender_pending_and_due_result_are_server_authoritative(self):
        for required in [
            "b.state=1 AND r.state=1",
            "JOIN faction_members m ON m.account_id=b.account_id",
            "closesAt<=gettime()",
            "CrimeTender_Finalize(tenderId)",
            "if(closesAt>gettime())",
            "ожидает решения NPC",
            "if(bidState==3)",
            "не выбрана NPC",
            "gDBPrimaryAccountId[playerid],Leader_PlayerOrg(playerid)",
        ]:
            self.assertIn(required,RECONNECT)
        self.assertIn('ORDER BY b.offer_code,b.bid_id LIMIT 1',TENDER)
        self.assertIn('WHERE tender_id=%d AND state=1',TENDER)
        self.assertIn("UNIQUE KEY uq_tender_one_pending",
                      (ROOT/"db/migrations/042_crime_tender_offers.sql").read_text(encoding="utf-8"))

    def test_ci_and_without_new_migration(self):
        self.assertIn("tests/crime_reconnect_mysql_uat.py",WORKFLOW)
        self.assertIn("version=46 LIMIT 1",CORE)
        self.assertNotIn("version=47",CORE)
        self.assertNotIn("047_crime",WORKFLOW)


if __name__=="__main__":
    unittest.main()
