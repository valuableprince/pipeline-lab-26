"""CRIME-03.1: shared rounds, safe first-winner settlement, solo fallback."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
SQL=(ROOT/"db/migrations/041_crime_competitive_contracts.sql").read_text("utf-8")
COMP=(ROOT/"gamemodes/includes/crime_competition.inc").read_text("utf-8")
UI=(ROOT/"gamemodes/includes/crime_comp_ui.inc").read_text("utf-8")
CORE=(ROOT/"gamemodes/includes/crime_contracts.inc").read_text("utf-8")
MAIN=(ROOT/"gamemodes/includes/crime_ui.inc").read_text("utf-8")
GAME=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
BUILD=(ROOT/".github/workflows/build.yml").read_text("utf-8")


class SharedCompetition(unittest.TestCase):
    def test_schema_three_rivals_and_hourly_rounds(self):
        for x in ("CREATE TABLE crime_competition_rounds", "uq_crime_comp_round",
                  "hour_bucket", "winner_contract_id", "winner_account_id",
                  "ADD COLUMN competition_id", "uq_crime_comp_faction",
                  "mode_id,reward_amount", "'RACE_LS_01'", "'RACE_SF_01'",
                  "'RACE_LV_01'", "VALUES (41)"):
            self.assertIn(x,SQL)
        self.assertIn("version=42 LIMIT 1",CORE)  # Newest gated schema
        self.assertIn("041_crime_competitive_contracts.sql",BUILD)
        self.assertIn("crime031_mysql_uat.py",BUILD)

    def test_shared_board_replenish_and_atomic_accept(self):
        for x in ("stock bool:CrimeComp_Refresh", "INSERT IGNORE INTO crime_competition_rounds",
                  "FLOOR(UNIX_TIMESTAMP()/3600)", "state=3,closed_at=UNIX_TIMESTAMP()",
                  "stock CrimeComp_Start(", "INSERT INTO crime_contracts",
                  "JOIN faction_members m", "p.trust>=t.required_trust",
                  "p.heat<70", "sc.standing>=5", "CRIME_COMP_MIN_LEFT",
                  "NOT EXISTS(SELECT 1 FROM crime_contracts h",
                  "CrimeContacts_Near(playerid,contact)", "Crime_Rollback()",
                  "Crime_Exec(\"COMMIT\")"):
            self.assertIn(x,COMP)
        self.assertIn("UNIQUE KEY uq_crime_comp_faction",SQL)
        self.assertIn("uq_crime_one_active_contract",
            (ROOT/"db/migrations/032_crime_contract_core.sql").read_text("utf-8"))

    def test_winner_claim_inside_wallet_transaction(self):
        for x in ("CrimeComp_RoundForContract(playerid,id,faction,competitionRound)",
                  "CrimeComp_Claim(playerid,competitionRound,id,faction)",
                  "INSERT INTO crime_contract_rewards",
                  "INSERT INTO crime_syndicate_influence_events",
                  "CASE WHEN c.mode_id=2 THEN 8", "CASE WHEN c.mode_id=2 THEN 5"):
            self.assertIn(x,CORE)
        block=CORE.split("stock Crime_Settle(",1)[1].split("stock Crime_Abort(",1)[0]
        self.assertLess(block.index('Crime_Exec("START TRANSACTION")'),
                        block.index("CrimeComp_Claim("))
        self.assertLess(block.index("CrimeComp_Claim("),
                        block.index("UPDATE accounts a JOIN crime_contracts"))
        self.assertLess(block.index("INSERT INTO crime_contract_rewards"),
                        block.index('Crime_Exec("COMMIT")'))
        self.assertIn("r.state=1 AND r.expires_at>UNIX_TIMESTAMP()",COMP)
        self.assertIn("WHERE competition_id=%d AND contract_id<>%d AND state IN(1,2)",COMP)

    def test_one_dialog_entry_and_secure_session(self):
        for x in ("COMP_DLG_BOARD  (7968)", "COMP_DLG_DETAIL (7969)",
                  "CrimeUI_Open(playerid,COMP_DLG_BOARD)",
                  "CrimeUI_Open(playerid,COMP_DLG_DETAIL)",
                  "gCrimeUIAuthToken[playerid]!=gAuthToken[playerid]",
                  "gCrimeUIAccountId[playerid]!=gDBPrimaryAccountId[playerid]",
                  "gCrimeUIDialog[playerid]=0",
                  "CrimeComp_Start(playerid,roundId)"):
            self.assertIn(x,UI)
        self.assertIn("Общие конкурсы",MAIN)
        self.assertIn("case 8: return CrimeCompUI_Board(playerid);",MAIN)
        self.assertIn('#include "crime_competition.inc"',GAME)
        self.assertIn('#include "crime_comp_ui.inc"',GAME)
        self.assertIn("CrimeCompUI_OnDialog(playerid, dialogid, response, listitem)",GAME)
        self.assertNotIn("cmd:crimecomp",GAME)


if __name__=="__main__":
    unittest.main()
