"""CRIME-03.2 disposable MariaDB UAT. Run after all CRIME-03.1 fixtures."""
import mysql.connector
from crime02_mysql_uat import CFG, run, scalar, begin, pickup
from crime023_mysql_uat import resolve

def rounds(db):
    run(db, """INSERT IGNORE INTO crime_tender_rounds
      (contact_id,template_id,bucket,opens_at,closes_at)
      SELECT t.contact_id,t.template_id,FLOOR(UNIX_TIMESTAMP()/600),
        UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+60
      FROM crime_contract_templates t JOIN crime_contacts n
        ON n.contact_id=t.contact_id AND n.is_enabled=1
      WHERE t.template_id IN(10,11,12) AND t.mode_id=3 AND t.is_enabled=1""")

def tender_id(db, contact):
    return scalar(db, """SELECT tender_id FROM crime_tender_rounds
      WHERE contact_id=%s AND bucket=FLOOR(UNIX_TIMESTAMP()/600)""", (contact,))

def bid(db, tid, acc, faction, choice):
    begin(db)
    try:
        n,_=run(db, """INSERT INTO crime_tender_bids
          (tender_id,account_id,faction_id,offer_code,submitted_at)
          SELECT r.tender_id,m.account_id,m.faction_id,%s,UNIX_TIMESTAMP()
          FROM crime_tender_rounds r JOIN crime_contract_templates t
            ON t.template_id=r.template_id AND t.mode_id=3 AND t.is_enabled=1
          JOIN faction_members m ON m.account_id=%s AND m.faction_id=%s
          JOIN crime_contact_progress p ON p.account_id=m.account_id
            AND p.contact_id=r.contact_id AND p.trust>=t.required_trust AND p.heat<70
          JOIN crime_syndicate_contacts sc ON sc.faction_id=m.faction_id
            AND sc.contact_id=r.contact_id AND sc.standing>=5
          WHERE r.tender_id=%s AND r.state=1 AND r.closes_at>UNIX_TIMESTAMP()
            AND NOT EXISTS(SELECT 1 FROM crime_contracts h
              WHERE h.account_id=m.account_id AND h.state IN(1,2))""",
            (choice,acc,faction,tid))
        if n!=1:
            run(db,"ROLLBACK")
            return False
        run(db,"COMMIT")
        return True
    except mysql.connector.IntegrityError as e:
        assert e.errno==1062,e
        run(db,"ROLLBACK")
        return False

def close(db, tid):
    run(db, """UPDATE crime_tender_rounds
      SET opens_at=UNIX_TIMESTAMP()-120,closes_at=UNIX_TIMESTAMP()-1
      WHERE tender_id=%s AND state=1""",(tid,))

def finalize(db,tid):
    begin(db)
    try:
        state=scalar(db,"SELECT state FROM crime_tender_rounds WHERE tender_id=%s FOR UPDATE",(tid,))
        if state!=1:
            run(db,"COMMIT")
            return None
        closes=scalar(db,"SELECT closes_at FROM crime_tender_rounds WHERE tender_id=%s",(tid,))
        if closes>scalar(db,"SELECT UNIX_TIMESTAMP()"):
            run(db,"COMMIT")
            return None
        cur=db.cursor()
        cur.execute("""SELECT b.bid_id,b.account_id,b.faction_id,b.offer_code
          FROM crime_tender_bids b
          JOIN crime_tender_rounds r ON r.tender_id=b.tender_id
          JOIN crime_contract_templates t ON t.template_id=r.template_id
             AND t.mode_id=3 AND t.is_enabled=1
          JOIN faction_members m ON m.account_id=b.account_id AND m.faction_id=b.faction_id
          JOIN crime_contact_progress p ON p.account_id=b.account_id
             AND p.contact_id=r.contact_id AND p.trust>=t.required_trust AND p.heat<70
          JOIN crime_syndicate_contacts sc ON sc.faction_id=b.faction_id
             AND sc.contact_id=r.contact_id AND sc.standing>=5
          WHERE b.tender_id=%s AND b.state=1
            AND NOT EXISTS(SELECT 1 FROM crime_contracts a
               WHERE a.account_id=b.account_id AND a.state IN(1,2))
          ORDER BY b.offer_code,b.bid_id LIMIT 1""",(tid,))
        chosen=cur.fetchone()
        cur.close()
        winner=chosen[0] if chosen else 0
        contract=None
        if chosen:
            bid_id,account,faction,code=chosen
            n,contract=run(db, """INSERT INTO crime_contracts
              (template_id,account_id,faction_id,mode_id,reward_amount,started_at,
               expires_at,contact_id,scenario_kind,scenario_event,tender_id)
              SELECT t.template_id,b.account_id,b.faction_id,3,
                FLOOR(t.reward_amount*(CASE b.offer_code
                  WHEN 1 THEN 70 WHEN 2 THEN 85 ELSE 100 END)/100),
                UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+(CASE b.offer_code
                  WHEN 1 THEN 900 WHEN 2 THEN 1500 ELSE 2400 END),
                r.contact_id,0,0,r.tender_id
              FROM crime_tender_bids b
              JOIN crime_tender_rounds r ON r.tender_id=b.tender_id AND r.state=1
              JOIN crime_contract_templates t ON t.template_id=r.template_id
                AND t.mode_id=3 AND t.is_enabled=1
              JOIN faction_members m ON m.account_id=b.account_id AND m.faction_id=b.faction_id
              JOIN crime_contact_progress p ON p.account_id=b.account_id
                AND p.contact_id=r.contact_id AND p.trust>=t.required_trust AND p.heat<70
              JOIN crime_syndicate_contacts sc ON sc.faction_id=b.faction_id
                AND sc.contact_id=r.contact_id AND sc.standing>=5
              WHERE b.bid_id=%s AND b.tender_id=%s AND b.account_id=%s
                AND b.faction_id=%s AND b.offer_code=%s AND b.state=1
                AND NOT EXISTS(SELECT 1 FROM crime_contracts h
                  WHERE h.account_id=b.account_id AND h.state IN(1,2))""",
                (bid_id,tid,account,faction,code))
            assert n==1
            assert run(db, """UPDATE crime_tender_rounds SET state=2,
                winner_bid_id=%s,winner_contract_id=%s,decided_at=UNIX_TIMESTAMP()
                WHERE tender_id=%s AND state=1""",(bid_id,contract,tid))[0]==1
            run(db, """INSERT INTO crime_contract_events
               (contract_id,actor_account_id,event_type)
               VALUES(%s,%s,'ACCEPTED')""",(contract,account))
        else:
            assert run(db, """UPDATE crime_tender_rounds SET state=3,
               decided_at=UNIX_TIMESTAMP()
               WHERE tender_id=%s AND state=1""",(tid,))[0]==1
        run(db, """UPDATE crime_tender_bids SET state=CASE WHEN bid_id=%s
            THEN 2 ELSE 3 END WHERE tender_id=%s AND state=1""",(winner,tid))
        run(db,"COMMIT")
        return contract
    except Exception:
        run(db,"ROLLBACK")
        raise

def penalty(db,cid,acc):
    begin(db)
    try:
        n,_=run(db, """INSERT IGNORE INTO crime_tender_penalties
          (contract_id,account_id,faction_id,contact_id,reason)
          SELECT contract_id,account_id,faction_id,contact_id,state
          FROM crime_contracts WHERE contract_id=%s AND account_id=%s
          AND mode_id=3 AND state IN(4,5)""",(cid,acc))
        if n:
            run(db, """UPDATE crime_contact_progress p
              JOIN crime_tender_penalties e
                ON e.account_id=p.account_id AND e.contact_id=p.contact_id
              SET p.trust=GREATEST(0,CAST(p.trust AS SIGNED)-3),
                  p.heat=LEAST(100,p.heat+5)
              WHERE e.contract_id=%s""",(cid,))
            run(db, """UPDATE crime_syndicate_contacts s
              JOIN crime_tender_penalties e
                ON e.faction_id=s.faction_id AND e.contact_id=s.contact_id
              SET s.standing=GREATEST(0,CAST(s.standing AS SIGNED)-2)
              WHERE e.contract_id=%s""",(cid,))
        run(db,"COMMIT")
        return n
    except Exception:
        run(db,"ROLLBACK")
        raise

def main():
    db=mysql.connector.connect(**CFG)
    try:
        a=scalar(db,"SELECT account_id FROM accounts WHERE username='CrimeSoloA'")
        b=scalar(db,"SELECT account_id FROM accounts WHERE username='CrimeSoloB'")
        assert a and b
        assert scalar(db,"SELECT COUNT(*) FROM schema_migrations WHERE version=42")==1
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_templates WHERE mode_id=3")==3
        rounds(db)
        assert scalar(db,"SELECT COUNT(*) FROM crime_tender_rounds WHERE bucket=FLOOR(UNIX_TIMESTAMP()/600)")==3
        race=tender_id(db,2)
        solo=tender_id(db,3)
        abort=tender_id(db,1)
        assert race and solo and abort
        # The faster, cheaper bid wins even if submitted second.
        assert bid(db,race,a,6,2)
        assert not bid(db,solo,a,6,1)  # One pending application per account.
        assert not bid(db,race,a,6,1)  # No edits or replay on faction application.
        assert bid(db,race,b,7,1)
        close(db,race)
        assert finalize(db,race)
        winner=scalar(db,"SELECT winner_contract_id FROM crime_tender_rounds WHERE tender_id=%s",(race,))
        assert scalar(db,"SELECT winner_bid_id FROM crime_tender_rounds WHERE tender_id=%s",(race,))==scalar(db,"SELECT bid_id FROM crime_tender_bids WHERE tender_id=%s AND account_id=%s",(race,b))
        assert scalar(db,"SELECT COUNT(*) FROM crime_contracts WHERE tender_id=%s",(race,))==1
        assert scalar(db,"SELECT account_id FROM crime_contracts WHERE contract_id=%s",(winner,))==b
        assert scalar(db,"SELECT reward_amount FROM crime_contracts WHERE contract_id=%s",(winner,))==2660
        assert finalize(db,race) is None
        assert scalar(db,"SELECT state FROM crime_tender_bids WHERE tender_id=%s AND account_id=%s",(race,a))==3
        before_b=scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(b,))
        before_a=scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))
        pickup(db,winner,b,7)
        assert resolve(db,winner,b,7,0)
        assert not resolve(db,winner,b,7,0)
        assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(b,))==before_b+2660
        assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))==before_a
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards WHERE contract_id=%s",(winner,))==1
        # Single online player: the NPC does not require rival bids.
        assert bid(db,solo,a,6,3)
        close(db,solo)
        sc=finalize(db,solo)
        assert sc
        assert scalar(db,"SELECT account_id FROM crime_contracts WHERE contract_id=%s",(sc,))==a
        assert scalar(db,"SELECT reward_amount FROM crime_contracts WHERE contract_id=%s",(sc,))==3700
        pickup(db,sc,a,6)
        assert resolve(db,sc,a,6,0)
        # A winning bid is a real delivery; an abandoned one has consequences.
        assert bid(db,abort,b,7,2)
        close(db,abort)
        failed=finalize(db,abort)
        assert failed
        trust=scalar(db,"SELECT trust FROM crime_contact_progress WHERE account_id=%s AND contact_id=1",(b,))
        heat=scalar(db,"SELECT heat FROM crime_contact_progress WHERE account_id=%s AND contact_id=1",(b,))
        standing=scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=7 AND contact_id=1")
        run(db,"UPDATE crime_contracts SET state=5,finished_at=UNIX_TIMESTAMP() WHERE contract_id=%s",(failed,))
        assert penalty(db,failed,b)==1
        assert penalty(db,failed,b)==0
        assert scalar(db,"SELECT trust FROM crime_contact_progress WHERE account_id=%s AND contact_id=1",(b,))==max(0,trust-3)
        assert scalar(db,"SELECT heat FROM crime_contact_progress WHERE account_id=%s AND contact_id=1",(b,))==min(100,heat+5)
        assert scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=7 AND contact_id=1")==max(0,standing-2)
        assert scalar(db,"SELECT COUNT(*) FROM crime_tender_penalties WHERE contract_id=%s",(failed,))==1
        # No-bid closing is final and does not create a contract.
        run(db,"""INSERT INTO crime_tender_rounds
          (contact_id,template_id,bucket,opens_at,closes_at)
          VALUES(1,10,FLOOR(UNIX_TIMESTAMP()/600)-1,
                 UNIX_TIMESTAMP()-120,UNIX_TIMESTAMP()-1)""")
        empty=scalar(db,"SELECT LAST_INSERT_ID()")
        assert finalize(db,empty) is None
        assert scalar(db,"SELECT state FROM crime_tender_rounds WHERE tender_id=%s",(empty,))==3
        assert scalar(db,"SELECT COUNT(*) FROM crime_contracts WHERE tender_id=%s",(empty,))==0
        print("CRIME-03.2 MariaDB UAT: PASS (price priority, replay, solo award, courier, penalty, no-bid expiry)")
    finally:
        db.close()

if __name__=="__main__":
    main()
