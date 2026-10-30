"""CRIME-03.5 disposable MariaDB UAT, after 034."""
import mysql.connector
from crime02_mysql_uat import CFG, run, scalar, begin


def issue(db, cid, account):
    return run(db, """INSERT INTO crime_intelligence_cases
      (contract_id,puzzle_code,expires_at)
      SELECT contract_id,1+MOD(contract_id,3),UNIX_TIMESTAMP()+1800
      FROM crime_contracts WHERE contract_id=%s AND account_id=%s
        AND mode_id=3 AND state=3""",(cid,account))[0]


def answer(db, cid, account, faction, answer_code):
    """Replicate SQL authority of the Pawn answer handler."""
    correct=scalar(db,"SELECT puzzle_code FROM crime_intelligence_cases WHERE contract_id=%s",(cid,))
    if answer_code==correct:
        n,_=run(db,"""UPDATE crime_intelligence_cases l
          JOIN crime_contracts c ON c.contract_id=l.contract_id
          JOIN faction_members m ON m.account_id=c.account_id AND m.faction_id=c.faction_id
          SET l.solved_at=UNIX_TIMESTAMP()
          WHERE l.contract_id=%s AND l.solved_at=0 AND l.decision=0
            AND l.next_attempt_at<=UNIX_TIMESTAMP() AND l.expires_at>UNIX_TIMESTAMP()
            AND c.account_id=%s AND c.faction_id=%s AND c.mode_id=3
            AND c.state=3 AND l.puzzle_code=%s""",(cid,account,faction,answer_code))
        return n==1
    n,_=run(db, """UPDATE crime_intelligence_cases l
       JOIN crime_contracts c ON c.contract_id=l.contract_id
       JOIN faction_members m ON m.account_id=c.account_id AND m.faction_id=c.faction_id
       SET l.attempts=l.attempts+1,l.next_attempt_at=UNIX_TIMESTAMP()+45
       WHERE l.contract_id=%s AND l.solved_at=0 AND l.decision=0
         AND l.attempts<65535 AND l.next_attempt_at<=UNIX_TIMESTAMP()
         AND l.expires_at>UNIX_TIMESTAMP() AND c.account_id=%s
         AND c.faction_id=%s AND c.mode_id=3 AND c.state=3
         AND l.puzzle_code<>%s""",(cid,account,faction,answer_code))
    return n==1


def redeem(db, cid, account, faction, contact, decision):
    begin(db)
    try:
        n,_=run(db, """UPDATE crime_intelligence_cases l
          JOIN crime_contracts c ON c.contract_id=l.contract_id
          JOIN faction_members m ON m.account_id=c.account_id AND m.faction_id=c.faction_id
          JOIN crime_contact_progress p ON p.account_id=c.account_id
            AND p.contact_id=c.contact_id
          JOIN crime_syndicate_progress s ON s.faction_id=c.faction_id
          JOIN crime_syndicate_contacts sc ON sc.faction_id=c.faction_id
            AND sc.contact_id=c.contact_id
          SET l.decision=%s,l.closed_at=UNIX_TIMESTAMP()
          WHERE l.contract_id=%s AND l.decision=0 AND l.closed_at=0
            AND l.solved_at>0 AND l.expires_at>UNIX_TIMESTAMP()
            AND c.account_id=%s AND c.faction_id=%s AND c.contact_id=%s
            AND c.mode_id=3 AND c.state=3""",
            (decision,cid,account,faction,contact))
        if n!=1:
            run(db,"ROLLBACK")
            return False
        if decision==1:
            run(db, """UPDATE crime_syndicate_progress p
              JOIN crime_contracts c ON c.faction_id=p.faction_id
              JOIN crime_intelligence_cases l ON l.contract_id=c.contract_id
                AND l.decision=1 AND l.closed_at>0
              SET p.influence=LEAST(1000000,p.influence+4)
              WHERE c.contract_id=%s AND c.account_id=%s AND c.faction_id=%s
                AND c.state=3""",(cid,account,faction))
            run(db, """UPDATE crime_syndicate_contacts p
              JOIN crime_contracts c ON c.faction_id=p.faction_id AND c.contact_id=p.contact_id
              JOIN crime_intelligence_cases l ON l.contract_id=c.contract_id
                AND l.decision=1 AND l.closed_at>0
              SET p.standing=LEAST(100,p.standing+1)
              WHERE c.contract_id=%s AND c.account_id=%s
                AND c.faction_id=%s AND c.state=3""",(cid,account,faction))
        else:
            run(db, """UPDATE crime_contact_progress p
              JOIN crime_contracts c ON c.account_id=p.account_id AND c.contact_id=p.contact_id
              JOIN crime_intelligence_cases l ON l.contract_id=c.contract_id
                AND l.decision=2 AND l.closed_at>0
              SET p.heat=GREATEST(0,CAST(p.heat AS SIGNED)-10),
                  p.trust=LEAST(100,p.trust+1)
              WHERE c.contract_id=%s AND c.account_id=%s
                AND c.faction_id=%s AND c.state=3""",(cid,account,faction))
        run(db,"""INSERT INTO crime_contract_events
          (contract_id,actor_account_id,event_type)
          VALUES(%s,%s,%s)""",
          (cid,account,"INTEL_ARCHIVED" if decision==1 else "INTEL_ERASED"))
        run(db,"COMMIT")
        return True
    except Exception:
        run(db,"ROLLBACK")
        raise


def main():
    db=mysql.connector.connect(**CFG)
    try:
        a=scalar(db,"SELECT account_id FROM accounts WHERE username='CrimeSoloA'")
        b=scalar(db,"SELECT account_id FROM accounts WHERE username='CrimeSoloB'")
        assert a and b and scalar(db,"SELECT COUNT(*) FROM schema_migrations WHERE version=45")==1
        cur=db.cursor()
        cur.execute("""SELECT contract_id,contact_id FROM crime_contracts
            WHERE account_id=%s AND faction_id=6 AND mode_id=3 AND state=3
            ORDER BY contract_id DESC LIMIT 3""",(a,))
        cases=cur.fetchall()
        cur.close()
        assert len(cases)==3,cases
        (archived,ca),(erased,ce),(expired,cx)=cases
        for cid,_ in cases:
            assert issue(db,cid,a)==1
            assert run(db, """INSERT IGNORE INTO crime_intelligence_cases
              (contract_id,puzzle_code,expires_at) VALUES(%s,1,UNIX_TIMESTAMP()+1800)""",(cid,))[0]==0
        assert not answer(db,archived,b,7,1+archived%3), "Other account cannot solve"
        assert not redeem(db,archived,a,6,ca,1), "Cannot redeem unsolved material"
        wrong=(1+archived%3)%3+1
        assert answer(db,archived,a,6,wrong)
        assert scalar(db,"SELECT attempts FROM crime_intelligence_cases WHERE contract_id=%s",(archived,))==1
        assert not answer(db,archived,a,6,1+archived%3), "45-second retry gate"
        assert scalar(db,"SELECT next_attempt_at FROM crime_intelligence_cases WHERE contract_id=%s",(archived,))>scalar(db,"SELECT UNIX_TIMESTAMP()")
        run(db,"UPDATE crime_intelligence_cases SET next_attempt_at=UNIX_TIMESTAMP()-1 WHERE contract_id=%s",(archived,))
        assert answer(db,archived,a,6,1+archived%3)
        assert not answer(db,archived,a,6,1+archived%3)
        before_wallet=scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))
        other=scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=7")
        influence=scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=6")
        standing=scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=6 AND contact_id=%s",(ca,))
        assert redeem(db,archived,a,6,ca,1)
        assert not redeem(db,archived,a,6,ca,1), "Claim once"
        assert scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=6")==min(1000000,influence+4)
        assert scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=6 AND contact_id=%s",(ca,))==min(100,standing+1)
        assert scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=7")==other
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_events WHERE contract_id=%s AND event_type='INTEL_ARCHIVED'",(archived,))==1
        assert answer(db,erased,a,6,1+erased%3)
        heat=scalar(db,"SELECT heat FROM crime_contact_progress WHERE account_id=%s AND contact_id=%s",(a,ce))
        trust=scalar(db,"SELECT trust FROM crime_contact_progress WHERE account_id=%s AND contact_id=%s",(a,ce))
        assert redeem(db,erased,a,6,ce,2)
        assert not redeem(db,erased,a,6,ce,1), "No alternative payout"
        assert scalar(db,"SELECT heat FROM crime_contact_progress WHERE account_id=%s AND contact_id=%s",(a,ce))==max(0,heat-10)
        assert scalar(db,"SELECT trust FROM crime_contact_progress WHERE account_id=%s AND contact_id=%s",(a,ce))==min(100,trust+1)
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_events WHERE contract_id=%s AND event_type='INTEL_ERASED'",(erased,))==1
        run(db,"UPDATE crime_intelligence_cases SET expires_at=UNIX_TIMESTAMP()-1 WHERE contract_id=%s",(expired,))
        assert not answer(db,expired,a,6,1+expired%3), "Expiry should fail closed"
        assert not redeem(db,expired,a,6,cx,2), "Expired case may never pay"
        assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))==before_wallet
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards WHERE contract_id IN(%s,%s,%s)",(archived,erased,expired))==3
        print("CRIME-03.5 MariaDB UAT: PASS (puzzles, cooldown, expiry, two outcomes, no wallet, once)")
    finally:
        db.close()


if __name__=="__main__":
    main()
