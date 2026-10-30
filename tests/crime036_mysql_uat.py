"""CRIME-03.6 disposable MariaDB UAT: high heat and one-player recovery."""
import mysql.connector
from crime02_mysql_uat import CFG, run, scalar, begin


def start(db,account,faction,contact,method):
    run(db,"""UPDATE crime_contact_recoveries SET state=4
       WHERE account_id=%s AND state IN(1,2)
         AND expires_at<=UNIX_TIMESTAMP()""",(account,))
    begin(db)
    try:
        n,rid=run(db, """INSERT INTO crime_contact_recoveries
           (account_id,faction_id,contact_id,method,started_at,expires_at)
           SELECT m.account_id,m.faction_id,p.contact_id,%s,
             UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+1200
           FROM faction_members m JOIN crime_contact_progress p
             ON p.account_id=m.account_id AND p.contact_id=%s AND p.heat>=60
           JOIN crime_syndicate_contacts sc ON sc.faction_id=m.faction_id
             AND sc.contact_id=p.contact_id
           WHERE m.account_id=%s AND m.faction_id=%s
             AND NOT EXISTS(SELECT 1 FROM crime_contact_recoveries r
               WHERE r.account_id=m.account_id AND r.state IN(1,2))
             AND NOT EXISTS(SELECT 1 FROM crime_contact_recoveries old
               WHERE old.account_id=m.account_id AND old.contact_id=p.contact_id
                 AND old.started_at>UNIX_TIMESTAMP()-3600)
             AND NOT EXISTS(SELECT 1 FROM crime_contracts c
               WHERE c.account_id=m.account_id AND c.state IN(1,2))""",
             (method,contact,account,faction))
        if n!=1:
            run(db,"ROLLBACK")
            return None
        run(db,"COMMIT")
        return rid
    except Exception:
        run(db,"ROLLBACK")
        raise


def field(db,rid,account,faction):
    n,_=run(db,"""UPDATE crime_contact_recoveries r
       JOIN faction_members m ON m.account_id=r.account_id
         AND m.faction_id=r.faction_id
       SET r.state=2,r.field_at=UNIX_TIMESTAMP()
       WHERE r.recovery_id=%s AND r.account_id=%s AND r.faction_id=%s
         AND r.state=1 AND r.field_at=0 AND r.expires_at>UNIX_TIMESTAMP()""",
         (rid,account,faction))
    return n==1


def finish(db,rid,account,faction):
    begin(db)
    try:
        n,_=run(db, """UPDATE crime_contact_recoveries r
           JOIN faction_members m ON m.account_id=r.account_id AND m.faction_id=r.faction_id
           JOIN crime_contact_progress p ON p.account_id=r.account_id AND p.contact_id=r.contact_id
           JOIN crime_syndicate_contacts sc ON sc.faction_id=r.faction_id AND sc.contact_id=r.contact_id
           SET r.state=3,r.finished_at=UNIX_TIMESTAMP(),r.heat_before=p.heat,
               r.heat_after=GREATEST(0,CAST(p.heat AS SIGNED)-
                 (CASE WHEN r.method=1 THEN 25 ELSE 35 END)),
               r.standing_before=sc.standing,
               r.standing_after=GREATEST(0,CAST(sc.standing AS SIGNED)-
                 (CASE WHEN r.method=2 THEN 3 ELSE 0 END))
           WHERE r.recovery_id=%s AND r.account_id=%s AND r.faction_id=%s
             AND r.state=2 AND r.field_at>0 AND r.finished_at=0
             AND r.expires_at>UNIX_TIMESTAMP()""",(rid,account,faction))
        if n!=1:
            run(db,"ROLLBACK")
            return False
        n,_=run(db, """UPDATE crime_contact_progress p
           JOIN crime_contact_recoveries r ON r.account_id=p.account_id AND r.contact_id=p.contact_id
           SET p.heat=r.heat_after,
               p.trust=LEAST(100,p.trust+(CASE WHEN r.method=1 THEN 1 ELSE 0 END))
           WHERE r.recovery_id=%s AND r.account_id=%s AND r.faction_id=%s
             AND r.state=3 AND r.finished_at>0""",(rid,account,faction))
        assert n==1
        n,_=run(db, """UPDATE crime_syndicate_contacts sc
           JOIN crime_contact_recoveries r ON r.faction_id=sc.faction_id AND r.contact_id=sc.contact_id
           SET sc.standing=r.standing_after
           WHERE r.recovery_id=%s AND r.account_id=%s AND r.faction_id=%s
             AND r.state=3 AND r.finished_at>0""",(rid,account,faction))
        assert n in (0,1), "Standing clamp may result in zero changed rows"
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
        assert a and b
        assert scalar(db,"SELECT COUNT(*) FROM schema_migrations WHERE version=46")==1
        before_wallet=scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))
        other=scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=7 AND contact_id=3")
        run(db,"UPDATE crime_contact_progress SET heat=85 WHERE account_id=%s AND contact_id=2",(a,))
        run(db,"UPDATE crime_contact_progress SET heat=90 WHERE account_id=%s AND contact_id=3",(a,))
        assert start(db,a,6,1,1) is None, "Contact introduction and heat gate"
        safe=start(db,a,6,2,1)
        assert safe
        assert start(db,a,6,3,2) is None, "One active mission across contacts"
        assert not field(db,safe,b,7), "Other faction must not finish field stage"
        assert not finish(db,safe,a,6), "Broker cannot skip field visit"
        trust=scalar(db,"SELECT trust FROM crime_contact_progress WHERE account_id=%s AND contact_id=2",(a,))
        standing=scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=6 AND contact_id=2")
        assert field(db,safe,a,6)
        assert not field(db,safe,a,6), "World Y is single-use"
        assert finish(db,safe,a,6)
        assert not finish(db,safe,a,6), "Cannot duplicate heat reduction"
        assert scalar(db,"SELECT heat FROM crime_contact_progress WHERE account_id=%s AND contact_id=2",(a,))==60
        assert scalar(db,"SELECT trust FROM crime_contact_progress WHERE account_id=%s AND contact_id=2",(a,))==min(100,trust+1)
        assert scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=6 AND contact_id=2")==standing
        assert scalar(db,"SELECT heat_before FROM crime_contact_recoveries WHERE recovery_id=%s",(safe,))==85
        assert scalar(db,"SELECT heat_after FROM crime_contact_recoveries WHERE recovery_id=%s",(safe,))==60
        assert start(db,a,6,2,1) is None, "60-minute per-contact cooldown"
        risky=start(db,a,6,3,2)
        assert risky
        trust3=scalar(db,"SELECT trust FROM crime_contact_progress WHERE account_id=%s AND contact_id=3",(a,))
        stand3=scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=6 AND contact_id=3")
        assert field(db,risky,a,6)
        assert finish(db,risky,a,6)
        assert scalar(db,"SELECT heat FROM crime_contact_progress WHERE account_id=%s AND contact_id=3",(a,))==55
        assert scalar(db,"SELECT trust FROM crime_contact_progress WHERE account_id=%s AND contact_id=3",(a,))==trust3
        assert scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=6 AND contact_id=3")==max(0,stand3-3)
        assert scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=7 AND contact_id=3")==other
        assert scalar(db,"SELECT COUNT(*) FROM crime_contact_recoveries WHERE state=3")==2
        # Fresh old records can expire, be cleaned and cannot grant a payout.
        run(db,"UPDATE crime_contact_progress SET heat=100 WHERE account_id=%s AND contact_id=2",(a,))
        run(db,"UPDATE crime_contact_recoveries SET started_at=UNIX_TIMESTAMP()-4000 WHERE recovery_id=%s",(safe,))
        abandoned=start(db,a,6,2,2)
        assert abandoned
        run(db,"UPDATE crime_contact_recoveries SET expires_at=UNIX_TIMESTAMP()-1 WHERE recovery_id=%s",(abandoned,))
        assert not field(db,abandoned,a,6)
        assert not finish(db,abandoned,a,6)
        assert scalar(db,"SELECT heat FROM crime_contact_progress WHERE account_id=%s AND contact_id=2",(a,))==100
        # Cleanup preserves audit state, but does not permit immediate replay.
        assert start(db,a,6,3,1) is None, "Contact 3 recently served"
        assert scalar(db,"SELECT state FROM crime_contact_recoveries WHERE recovery_id=%s",(abandoned,))==4
        assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))==before_wallet
        print("CRIME-03.6 MariaDB UAT: PASS (two missions, time, cooldown, faction, idempotent, no money)")
    finally:
        db.close()


if __name__=="__main__":
    main()
