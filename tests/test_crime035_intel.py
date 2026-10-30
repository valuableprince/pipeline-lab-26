"""CRIME-03.5 integration: optional NPC intelligence cannot mint cash."""
import unittest
from pathlib import Path

R=Path(__file__).resolve().parents[1]
DB=(R/"db/migrations/045_crime_intelligence_cases.sql").read_text("utf-8")
C=(R/"gamemodes/includes/crime_contracts.inc").read_text("utf-8")
E=(R/"gamemodes/includes/crime_intel.inc").read_text("utf-8")
UI=(R/"gamemodes/includes/crime_intel_ui.inc").read_text("utf-8")
MAIN=(R/"gamemodes/includes/crime_ui.inc").read_text("utf-8")
GAME=(R/"gamemodes/signal0317.pwn").read_text("utf-8")
BUILD=(R/".github/workflows/build.yml").read_text("utf-8")

class Intel(unittest.TestCase):
    def test_migration_and_single_source_of_truth(self):
        for s in ("CREATE TABLE crime_intelligence_cases",
                  "contract_id INT UNSIGNED NOT NULL PRIMARY KEY",
                  "puzzle_code TINYINT","next_attempt_at","closed_at",
                  "expires_at INT UNSIGNED","chk_crime_intel_closed",
                  "VALUES (45)", "c.finished_at+1800"):
            self.assertIn(s,DB)
        self.assertIn("version=46 LIMIT 1",C)
        self.assertIn("045_crime_intelligence_cases.sql",BUILD)
        self.assertIn("crime035_mysql_uat.py",BUILD)
        self.assertIn("CRIME_INTEL_DLG_BOARD (7975)",E)
        self.assertIn("CRIME_INTEL_DLG_PUZZLE (7976)",E)
        self.assertIn("CRIME_INTEL_DLG_DECISION (7977)",E)

    def test_issued_inside_wallet_transaction(self):
        settle=C.split("stock Crime_Settle(",1)[1].split("stock Crime_Abort(",1)[0]
        self.assertIn("INSERT INTO crime_intelligence_cases(contract_id,puzzle_code,expires_at)",settle)
        self.assertIn("UNIX_TIMESTAMP()+1800",settle)
        self.assertIn("mode_id=3 AND state=3",settle)
        self.assertLess(settle.index("INSERT INTO crime_contract_rewards"),
                        settle.index("INSERT INTO crime_intelligence_cases"))
        self.assertLess(settle.index("INSERT INTO crime_intelligence_cases"),
                        settle.rindex('Crime_Exec("COMMIT")'))

    def test_secure_puzzles_and_one_time_effects(self):
        for s in ("stock CrimeIntel_Answer(","stock bool:CrimeIntel_Redeem(",
                  "l.next_attempt_at=UNIX_TIMESTAMP()+45",
                  "l.attempts<65535",
                  "l.puzzle_code=%d","l.puzzle_code<>%d",
                  "l.decision=0 AND l.closed_at=0 AND l.solved_at>0",
                  "JOIN faction_members m","CrimeContacts_Near(playerid,contact)",
                  "gGovTransferBusy[playerid]","IsPlayerInAnyVehicle(playerid)",
                  'Crime_Exec("START TRANSACTION")','Crime_Rollback()',
                  "p.influence=LEAST(1000000,p.influence+4)",
                  "p.standing=LEAST(100,p.standing+1)",
                  "p.heat=GREATEST(0,CAST(p.heat AS SIGNED)-10)",
                  "p.trust=LEAST(100,p.trust+1)",
                  "INTEL_ARCHIVED","INTEL_ERASED"):
            self.assertIn(s,E)
        self.assertNotIn("UPDATE accounts",E)
        self.assertNotIn("crime_contract_rewards(",E)
        self.assertIn("CrimeIntelUI_OnDialog(playerid, dialogid, response, listitem)",GAME)
        self.assertIn("CrimeIntel_Reset(playerid);",GAME)
        self.assertIn('#include "crime_intel_ui.inc"',GAME)
        self.assertIn("gCrimeUIAuthToken[playerid]!=gAuthToken[playerid]",UI)
        self.assertIn("gCrimeUIAccountId[playerid]!=gDBPrimaryAccountId[playerid]",UI)
        self.assertIn("gCrimeUIDialog[playerid]=0",UI)

    def test_menu_and_solo_flow(self):
        self.assertIn("Оперативные сводки",MAIN)
        self.assertIn("case 10: return CrimeIntelUI_Board(playerid);",MAIN)
        self.assertIn("listitem > 12",MAIN)
        self.assertIn("Количество груза",UI)
        self.assertIn("Номер пломбы",UI)
        self.assertIn("Время отправления",UI)
        self.assertIn("Стереть следы",UI)
        self.assertIn("Передать в архив синдиката",UI)

if __name__=="__main__":
    unittest.main()
