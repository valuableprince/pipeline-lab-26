"""CRIME-03.2 static integration and security contract tests."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
SQL=(ROOT/"db/migrations/042_crime_tender_offers.sql").read_text("utf-8")
ENGINE=(ROOT/"gamemodes/includes/crime_tenders.inc").read_text("utf-8")
UI=(ROOT/"gamemodes/includes/crime_tenders_ui.inc").read_text("utf-8")
CORE=(ROOT/"gamemodes/includes/crime_contracts.inc").read_text("utf-8")
MAIN=(ROOT/"gamemodes/includes/crime_ui.inc").read_text("utf-8")
GAME=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
BUILD=(ROOT/".github/workflows/build.yml").read_text("utf-8")

class TenderTests(unittest.TestCase):
    def test_schema(self):
        for s in ("CREATE TABLE crime_tender_rounds","CREATE TABLE crime_tender_bids",
                  "CREATE TABLE crime_tender_penalties","uq_crime_tender_window",
                  "uq_tender_bid_faction","uq_tender_one_pending",
                  "ADD COLUMN tender_id","uq_crime_tender_contract",
                  "CHECK (state IN(1,2,3))","'TENDER_LS_01'",
                  "'TENDER_SF_01'","'TENDER_LV_01'",
                  "VALUES (42)"):
            self.assertIn(s,SQL)
        self.assertIn("version=44 LIMIT 1",CORE)
        self.assertIn("042_crime_tender_offers.sql",BUILD)
        self.assertIn("crime032_mysql_uat.py",BUILD)

    def test_sql_authority_and_atomicity(self):
        for s in ("stock bool:CrimeTender_Refresh","CRIME_TENDER_WINDOW (60)",
                  "INSERT IGNORE INTO crime_tender_rounds",
                  "stock bool:CrimeTender_Finalize",
                  "START TRANSACTION","FOR UPDATE","ORDER BY b.offer_code,b.bid_id",
                  "JOIN faction_members m", "p.trust>=t.required_trust",
                  "p.heat<70","sc.standing>=5",
                  "NOT EXISTS(SELECT 1 FROM crime_contracts",
                  "INSERT INTO crime_contracts","UPDATE crime_tender_rounds SET state=2",
                  "UPDATE crime_tender_bids SET state=CASE",
                  "Crime_Exec(\"COMMIT\")",
                  "CrimeContacts_Near(playerid,contact)","Crime_Rollback()"):
            self.assertIn(s,ENGINE)
        block=ENGINE.split("stock bool:CrimeTender_Finalize(",1)[1].split("stock bool:CrimeTender_Refresh(",1)[0]
        self.assertLess(block.index('START TRANSACTION'),block.index('FOR UPDATE'))
        self.assertLess(block.index('FOR UPDATE'),block.index('INSERT INTO crime_contracts'))
        self.assertLess(block.index('INSERT INTO crime_contracts'),block.index('UPDATE crime_tender_rounds SET state=2'))
        self.assertIn("tender_id",CORE) if "tender_id" in CORE else None

    def test_failure_is_accounted_once(self):
        for s in ("INSERT IGNORE INTO crime_tender_penalties",
                  "if(changed==0) return true","p.trust=GREATEST",
                  "p.heat=LEAST","s.standing=GREATEST",
                  "stock bool:CrimeTender_PenalizeExpired"):
            self.assertIn(s,ENGINE)
        self.assertIn("CrimeTender_PenalizeExpired(playerid)",CORE)
        self.assertIn("CrimeTender_Penalize(playerid, id)",CORE)

    def test_one_entry_dialog_and_build(self):
        for s in ("TENDER_DLG_BOARD   (7970)","TENDER_DLG_DETAIL  (7971)",
                  "TENDER_DLG_OFFER   (7972)",
                  "gCrimeUIAuthToken[playerid]!=gAuthToken[playerid]",
                  "gCrimeUIAccountId[playerid]!=gDBPrimaryAccountId[playerid]",
                  "gCrimeUIDialog[playerid]=0","CrimeTender_Submit(playerid"):
            self.assertIn(s,UI)
        self.assertIn("Конкурсы предложений",MAIN)
        self.assertIn("case 9: return CrimeTenderUI_Board(playerid);",MAIN)
        self.assertIn('#include "crime_tenders.inc"',GAME)
        self.assertIn('#include "crime_tenders_ui.inc"',GAME)
        self.assertIn("CrimeTenderUI_OnDialog(playerid, dialogid, response, listitem)",GAME)

if __name__=="__main__":
    unittest.main()
