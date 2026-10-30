"""CRIME-02.8 faction influence and NPC relationship feature guards."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
SQL=(ROOT/"db/migrations/039_crime_syndicate_influence.sql").read_text("utf-8")
CORE=(ROOT/"gamemodes/includes/crime_contracts.inc").read_text("utf-8")
ORG=(ROOT/"gamemodes/includes/crime_org.inc").read_text("utf-8")
UI=(ROOT/"gamemodes/includes/crime_ui.inc").read_text("utf-8")
MODE=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
WORKFLOW=(ROOT/".github/workflows/build.yml").read_text("utf-8")

class SyndicateInfluence(unittest.TestCase):
    def test_schema(self):
        for v in ("crime_syndicate_progress","crime_syndicate_contacts",
                  "crime_syndicate_assets","crime_syndicate_influence_events",
                  "PRIMARY KEY (faction_id,contact_id,asset_code)",
                  "contract_id INT UNSIGNED NOT NULL PRIMARY KEY",
                  "faction_id IN (6,7,8)","VALUES (39)"):
            self.assertIn(v,SQL)
        self.assertIn("version=39 LIMIT 1",CORE)
        self.assertIn("039_crime_syndicate_influence.sql",WORKFLOW)
        self.assertIn("crime028_mysql_uat.py",WORKFLOW)
    def test_atomic_earn(self):
        block=CORE.split("stock Crime_Settle(",1)[1].split("stock Crime_Abort(",1)[0]
        for v in ("INSERT INTO crime_contract_rewards", "INSERT INTO crime_contact_trust_events",
                  "INSERT INTO crime_contact_heat_events", "INSERT INTO crime_syndicate_influence_events",
                  "UPDATE crime_syndicate_progress", "UPDATE crime_syndicate_contacts",
                  'Crime_Exec("COMMIT")', "Crime_Rollback()"):
            self.assertIn(v,block)
        self.assertLess(block.index("INSERT INTO crime_contract_rewards"),block.index("INSERT INTO crime_syndicate_influence_events"))
        self.assertLess(block.index("INSERT INTO crime_syndicate_influence_events"),block.index('Crime_Exec("COMMIT")'))
        self.assertIn("a.asset_code=1",block)
        self.assertIn("a.asset_code=2",block)
    def test_purchase(self):
        for v in ("Leader_PlayerRank(playerid) < 4", "m.rank_id>=4",
                  "p.influence=p.influence-%d", "p.influence>=%d",
                  "NOT EXISTS(SELECT 1 FROM crime_syndicate_assets",
                  "INSERT INTO crime_syndicate_assets", "Crime_Rollback()",
                  'Crime_Exec("START TRANSACTION")', 'Crime_Exec("COMMIT")',
                  "CrimeOrg_AssetCost", "CrimeOrg_AssetStanding"):
            self.assertIn(v,ORG)
    def test_ui_and_include(self):
        for v in ("CRIME_DLG_ORG", "CRIME_DLG_ORG_CONTACT",
                  "CRIME_DLG_ORG_ASSETS", "CRIME_DLG_ORG_BUY",
                  "gCrimeUIOrgContact[playerid]", "gCrimeUIOrgAsset[playerid]",
                  "CrimeOrg_Buy(playerid, cid, code)","Синдикат и активы"):
            self.assertIn(v,UI)
        self.assertIn('#include "crime_org.inc"',MODE)
        self.assertNotIn("cmd:crimeorg",MODE)

if __name__=="__main__":
    unittest.main()
