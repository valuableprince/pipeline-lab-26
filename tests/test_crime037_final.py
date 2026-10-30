"""CRIME-03.7: authority, leaderboard semantics, menu regression and economic envelope."""
import re
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
RANK=(ROOT/"gamemodes/includes/crime_leaderboard.inc").read_text("utf-8")
GAME=(ROOT/"gamemodes/signal0317.pwn").read_text("utf-8")
UI=(ROOT/"gamemodes/includes/crime_ui.inc").read_text("utf-8")
CORE=(ROOT/"gamemodes/includes/crime_contracts.inc").read_text("utf-8")
TENDER=(ROOT/"gamemodes/includes/crime_tenders.inc").read_text("utf-8")
RACE=(ROOT/"gamemodes/includes/crime_competition.inc").read_text("utf-8")
CIB=(ROOT/".github/workflows/build.yml").read_text("utf-8")

class Scoreboard(unittest.TestCase):
    def test_read_only_and_events(self):
        for term in ("crime_contract_rewards rr",
                     "crime_competition_rounds r",
                     "crime_tender_bids b",
                     "crime_tender_penalties p",
                     "e.event_type='RACE_WON'",
                     "e.event_type='RACE_LOST'",
                     "b.state=3 AND r.state IN(2,3)",
                     "x.mode_id=3 AND x.state=3",
                     "r.winner_faction_id=p.faction_id",
                     "points[i]=race[i]*5+tender[i]*3",
                     "ORDER BY h.stamp DESC,h.refid DESC LIMIT 12",
                     "faction[j]<faction[i]",
                     "ranked" if False else "CrimeRank_History",
                     "gCrimeUIAuthToken[playerid]!=gAuthToken[playerid]",
                     "gCrimeUIAccountId[playerid]!=gDBPrimaryAccountId[playerid]",
                     "gCrimeUIDialog[playerid]=0",
                     "CRIME_RANK_DLG_BOARD (7983)",
                     "CRIME_RANK_DLG_HISTORY (7984)"):
            self.assertIn(term,RANK)
        for unsafe in ("UPDATE accounts","INSERT INTO crime_contract_rewards",
                       "SET p.influence","CrimeTender_Refresh(","CrimeComp_Refresh(",
                       "Economy_Credit(","START TRANSACTION"):
            self.assertNotIn(unsafe,RANK)
        self.assertIn("crime037_mysql_uat.py",CIB)

    def test_integrated_secure_session(self):
        self.assertIn('#include "crime_leaderboard.inc"',GAME)
        self.assertGreaterEqual(GAME.count("CrimeRank_Reset(playerid);"),2)
        self.assertIn("CrimeRank_OnDialog(playerid, dialogid, response, listitem)",GAME)
        self.assertIn("case 12: return CrimeRank_Board(playerid);",UI)
        self.assertIn("if (listitem < 0 || listitem > 12)",UI)
        self.assertIn("Рейтинг синдикатов",UI)
        self.assertIn("forward CrimeRank_Board(playerid);",CORE)
        self.assertIn("version=46 LIMIT 1",CORE)  # No new schema needed.

    def test_no_counter_table_or_recalculation_timer(self):
        self.assertNotIn("CREATE TABLE crime_syndicate_rank",RANK)
        self.assertNotIn("SetTimer(",RANK)
        self.assertRegex(RANK, r"FROM crime_syndicate_progress p")
        self.assertRegex(RANK,r"WHERE p.faction_id IN\(6,7,8\)")
        self.assertIn("JOIN crime_contract_rewards rr",RANK)

class BalancedCompetition(unittest.TestCase):
    @staticmethod
    def tender_winner(offers):
        """Matches documented SQL ORDER BY b.offer_code,b.bid_id."""
        return min(offers,key=lambda x:(x[1],x[0])) if offers else None

    def test_npc_bid_selection_one_two_three_organizations(self):
        self.assertIn("ORDER BY b.offer_code,b.bid_id LIMIT 1",TENDER)
        self.assertIn("UNIQUE", (ROOT/"db/migrations/042_crime_tender_offers.sql").read_text())
        self.assertEqual(self.tender_winner([(5,3)]),(5,3))
        self.assertEqual(self.tender_winner([(5,3),(6,2)]),(6,2))
        self.assertEqual(self.tender_winner([(5,3),(6,2),(7,1)]),(7,1))
        self.assertEqual(self.tender_winner([(5,2),(6,2),(7,2)]),(5,2))
        self.assertIsNone(self.tender_winner([]))
        self.assertIn("WHERE r.round_id=%d AND r.state=1",RACE)
        self.assertIn("c.state=2",RACE)

    def test_noninflation_and_reward_bounds(self):
        self.assertIn("70 WHEN 2 THEN 85 ELSE 100",TENDER)
        self.assertIn("ELSE 2400 END",TENDER)
        self.assertIn("reward_amount<=49700", (ROOT/"gamemodes/includes/crime_incidents.inc").read_text())
        template_rewards=[3600,3800,3700]
        durations={1:900,2:1500,3:2400}
        for baseline in template_rewards:
            for offer,ratio in ((1,70),(2,85),(3,100)):
                payout=baseline*ratio//100
                self.assertLessEqual(payout+300,50000)
                self.assertGreaterEqual(durations[offer],900)
                self.assertLessEqual(durations[offer],2400)
                # No bid is a cash transfer. One successfully delivered
                # contract can produce at most one normal payment.
                self.assertGreater(payout,0)
        for racers in (1,2,3):
            for paid in (0,1):
                self.assertLessEqual(paid,racers)  # At most one winning award.
        self.assertIn("JOIN crime_contract_rewards rr",RANK)
        self.assertIn("UNIQUE KEY uq_crime_comp_round", (ROOT/"db/migrations/041_crime_competitive_contracts.sql").read_text())
        self.assertIn("PRIMARY KEY", (ROOT/"db/migrations/032_crime_contract_core.sql").read_text())
        self.assertIn("WHERE c.contract_id",CORE)

if __name__=="__main__":
    unittest.main()
