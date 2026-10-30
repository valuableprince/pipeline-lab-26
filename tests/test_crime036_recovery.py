"""CRIME-03.6 static fail-closed contract: solo recovery missions."""
import unittest
from pathlib import Path
R=Path(__file__).resolve().parents[1]
SQL=(R/"db/migrations/046_crime_contact_recovery.sql").read_text("utf-8")
CORE=(R/"gamemodes/includes/crime_contracts.inc").read_text("utf-8")
ENGINE=(R/"gamemodes/includes/crime_recovery.inc").read_text("utf-8")
UI=(R/"gamemodes/includes/crime_recovery_ui.inc").read_text("utf-8")
MENU=(R/"gamemodes/includes/crime_ui.inc").read_text("utf-8")
GAME=(R/"gamemodes/signal0317.pwn").read_text("utf-8")
CI=(R/".github/workflows/build.yml").read_text("utf-8")

class RecoveryContract(unittest.TestCase):
    def test_schema_and_readiness(self):
        for token in ("CREATE TABLE crime_contact_recoveries",
                      "UNIQUE KEY uq_crime_one_active_recovery",
                      "KEY idx_crime_recovery_cooldown",
                      "active_account_id BIGINT", "started_at", "expires_at",
                      "heat_before","heat_after","standing_before","standing_after",
                      "chk_crime_recovery_stages","VALUES (46)"):
            self.assertIn(token,SQL)
        self.assertIn("version=46 LIMIT 1",CORE)
        self.assertIn("046_crime_contact_recovery.sql",CI)
        self.assertIn("crime036_mysql_uat.py",CI)

    def test_single_active_cooldown_and_no_cash(self):
        for token in ("stock bool:CrimeRecovery_Start(",
                      "Crime_Exec(\"START TRANSACTION\")",
                      "state=4","expires_at<=UNIX_TIMESTAMP()",
                      "p.heat>=60","NOT EXISTS(SELECT 1 FROM crime_contact_recoveries r",
                      "old.started_at>UNIX_TIMESTAMP()-3600",
                      "NOT EXISTS(SELECT 1 FROM crime_contracts c",
                      "UNIX_TIMESTAMP()+1200","state IN(1,2)",
                      "JOIN faction_members m","CrimeContacts_Near(playerid,contact)",
                      "IsPlayerInAnyVehicle(playerid)", "Crime_Rollback()",
                      "stock bool:CrimeRecovery_FieldComplete(",
                      "r.field_at=UNIX_TIMESTAMP()",
                      "stock bool:CrimeRecovery_Finish(",
                      "r.heat_before=p.heat","r.heat_after=GREATEST(0",
                      "r.standing_before=sc.standing",
                      "r.standing_after=GREATEST(0",
                      "p.heat=r.heat_after","p.trust=LEAST(100",
                      "sc.standing=r.standing_after"):
            self.assertIn(token,ENGINE)
        self.assertNotIn("UPDATE accounts",ENGINE)
        self.assertNotIn("Economy_Credit(",ENGINE)
        self.assertNotIn("gAccountMoney[playerid]",ENGINE)

    def test_secure_world_and_no_extra_commands(self):
        for token in ("CreateActor(","DestroyActor(","KEY_YES",
                      "gCrimeUIDialog[playerid]!=0",
                      "CRIME_RECOVERY_RADIUS (3.5)",
                      "GetPlayerInterior(playerid)!=0",
                      "GetPlayerVirtualWorld(playerid)!=0",
                      "CrimeRecovery_Info(playerid,id,contact,method,stage,deadline)",
                      "CrimeUI_Open(playerid,CRIME_RECOVERY_DLG_FIELD)"):
            self.assertIn(token,ENGINE)
        for token in ("gCrimeUIAuthToken[playerid]!=gAuthToken[playerid]",
                      "gCrimeUIAccountId[playerid]!=gDBPrimaryAccountId[playerid]",
                      "gCrimeUIDialog[playerid]=0",
                      "CrimeRecovery_Start(playerid,contact,listitem+1)",
                      "CrimeRecovery_FieldComplete(playerid,id)",
                      "CrimeRecovery_Finish(playerid,id)"):
            self.assertIn(token,UI)
        for token in ("#include \"crime_recovery.inc\"",
                      "#include \"crime_recovery_ui.inc\"",
                      "CrimeRecovery_OnKeys(playerid, newkeys, oldkeys)",
                      "CrimeRecoveryUI_OnDialog(playerid, dialogid, response, listitem)",
                      "CrimeRecovery_Reset(playerid)",
                      "CrimeRecovery_Shutdown()"):
            self.assertIn(token,GAME)
        self.assertIn("case 11: return CrimeRecoveryUI_Board(playerid);",MENU)
        self.assertIn("if (listitem < 0 || listitem > 11)",MENU)
        self.assertIn("Восстановление связи",MENU)
        self.assertNotIn("CMD:recover",GAME)

if __name__=="__main__":
    unittest.main()
