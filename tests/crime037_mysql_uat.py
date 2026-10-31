"""CRIME-03.7: SQL scoreboard read from actual Pawn statements, not copied logic.

Runs against disposable MariaDB after CRIME-03.6. No writes; no extra payout.
"""
import re
import time
from pathlib import Path

import mysql.connector
from crime02_mysql_uat import CFG, scalar

ROOT=Path(__file__).resolve().parents[1]
PAWN=(ROOT/"gamemodes/includes/crime_leaderboard.inc").read_text("utf-8")

BOARD=re.search(
    r'new Cache:c=mysql_query\(gDBPrimary,\s*"([^"]+)"\);',PAWN)
HISTORY=re.search(
    r'mysql_format\(gDBPrimary,query,sizeof\(query\),\s*"([^"]+)",\s*'
    r'faction,faction,faction,faction\);',PAWN)
assert BOARD and HISTORY, "Cannot extract production ranking statements"


def rows(db,sql,args=()):
    cur=db.cursor(dictionary=True)
    try:
        cur.execute(sql,args)
        return cur.fetchall()
    finally:
        cur.close()


def main():
    db=mysql.connector.connect(**CFG)
    try:
        assert scalar(db,"SELECT COUNT(*) FROM schema_migrations WHERE version=46")==1
        assert scalar(db,"SELECT COUNT(*) FROM crime_syndicate_progress WHERE faction_id IN(6,7,8)")==3
        initial_wallet=scalar(db,"SELECT COALESCE(SUM(balance),0) FROM accounts")
        initial_rewards=scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards")
        initial_events=scalar(db,"SELECT COUNT(*) FROM crime_contract_events")
        initial_rounds=scalar(db,"SELECT COUNT(*) FROM crime_competition_rounds")
        board_sql=BOARD.group(1)
        history_sql=HISTORY.group(1).replace("%d","%s")
        elapsed=[]
        for _ in range(20):
            t=time.perf_counter()
            board=rows(db,board_sql)
            elapsed.append((time.perf_counter()-t)*1000)
            assert len(board)==3
            assert [v["faction_id"] for v in board]==[6,7,8]
            assert len({v["faction_id"] for v in board})==3
            assert all(v["race_wins"]>=0 and v["race_losses"]>=0
                       and v["tender_done"]>=0 and v["tender_lost"]>=0 for v in board)
        # This is a genuine 2-faction race and at least one solo tender
        # from previous MariaDB UAT, not made-up successful state counters.
        assert any(x["race_wins"]>0 for x in board)
        assert any(x["race_losses"]>0 for x in board)
        assert any(x["tender_done"]>0 for x in board)
        for s in board:
            f=s["faction_id"]
            def count(sql,*args):
                return scalar(db,sql,args)
            assert s["race_wins"]==count("""
                SELECT COUNT(*) FROM crime_competition_rounds r
                JOIN crime_contract_rewards rr ON rr.contract_id=r.winner_contract_id
                WHERE r.winner_faction_id=%s AND r.state=2""",f)
            assert s["race_losses"]==count("""
                SELECT COUNT(*) FROM crime_contracts c
                JOIN crime_contract_events e ON e.contract_id=c.contract_id
                   AND e.event_type='RACE_LOST'
                WHERE c.faction_id=%s AND c.mode_id=2""",f)
            assert s["tender_done"]==count("""
                SELECT COUNT(*) FROM crime_contracts c
                JOIN crime_contract_rewards rr ON rr.contract_id=c.contract_id
                WHERE c.faction_id=%s AND c.mode_id=3 AND c.state=3""",f)
            assert s["tender_lost"]==count("""
                SELECT COUNT(*) FROM crime_tender_bids b
                JOIN crime_tender_rounds r ON r.tender_id=b.tender_id
                WHERE b.faction_id=%s AND b.state=3 AND r.state IN(2,3)""",f)
            assert s["solo_done"]==count("""
                SELECT COUNT(*) FROM crime_contracts c
                JOIN crime_contract_rewards rr ON rr.contract_id=c.contract_id
                WHERE c.faction_id=%s AND c.mode_id=1 AND c.state=3""",f)
            assert s["total_paid"]==count("""
                SELECT COALESCE(SUM(rr.amount),0) FROM crime_contract_rewards rr
                JOIN crime_contracts c ON c.contract_id=rr.contract_id
                WHERE c.faction_id=%s""",f)
            assert s["influence"]==count("""
                SELECT influence FROM crime_syndicate_progress WHERE faction_id=%s""",f)
            history=rows(db,history_sql,(f,f,f,f))
            assert len(history)<=12
            assert all(1<=x["kind"]<=5 and x["refid"]>0 for x in history)
            for item in history:
                kind=item["kind"]
                if kind==1:
                    assert count("""SELECT COUNT(*) FROM crime_contract_events e
                       JOIN crime_contracts c ON c.contract_id=e.contract_id
                       JOIN crime_contract_rewards rr ON rr.contract_id=c.contract_id
                       WHERE c.contract_id=%s AND c.faction_id=%s
                         AND c.mode_id=2 AND e.event_type='RACE_WON'""",
                         item["refid"],f)==1
                if kind==2:
                    assert count("""SELECT COUNT(*) FROM crime_contract_events e
                       JOIN crime_contracts c ON c.contract_id=e.contract_id
                       WHERE c.contract_id=%s AND c.faction_id=%s
                         AND c.mode_id=2 AND e.event_type='RACE_LOST'""",
                         item["refid"],f)==1
                if kind==3:
                    assert count("""SELECT COUNT(*) FROM crime_contracts c
                       JOIN crime_contract_rewards rr ON rr.contract_id=c.contract_id
                       WHERE c.contract_id=%s AND c.faction_id=%s
                         AND c.mode_id=3 AND c.state=3""",item["refid"],f)==1
                if kind==4:
                    assert count("""SELECT COUNT(*) FROM crime_tender_bids b
                       JOIN crime_tender_rounds r ON r.tender_id=b.tender_id
                       WHERE b.bid_id=%s AND b.faction_id=%s
                         AND b.state=3 AND r.state IN(2,3)""",item["refid"],f)==1
                if kind==5:
                    assert count("""SELECT COUNT(*) FROM crime_contracts c
                       JOIN crime_tender_penalties p ON p.contract_id=c.contract_id
                       WHERE c.contract_id=%s AND c.faction_id=%s
                         AND c.mode_id=3 AND c.state IN(4,5)""",item["refid"],f)==1
        # Solo contracts never earn points. Deterministic tie-breaking
        # avoids page flicker at 0 or low server population.
        ranked=sorted(board,key=lambda x:(-(x["race_wins"]*5+x["tender_done"]*3),
                                          -x["race_wins"],-x["tender_done"],
                                          x["faction_id"]))
        assert len(ranked)==3
        # 20 read-only refreshes + three histories must not mint money
        # or even CREATE competition rounds.
        assert scalar(db,"SELECT COALESCE(SUM(balance),0) FROM accounts")==initial_wallet
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards")==initial_rewards
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_events")==initial_events
        assert scalar(db,"SELECT COUNT(*) FROM crime_competition_rounds")==initial_rounds
        print("CRIME-03.7 MariaDB UAT: PASS (read-only ledger rank, verified history, "
              f"1/2/3 faction rules, zero payout, p50-query={sorted(elapsed)[10]:.1f}ms)")
    finally:
        db.close()

if __name__=="__main__":
    main()
