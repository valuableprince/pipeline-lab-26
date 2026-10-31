"""CRIME-03.1: public hourly pool, racing syndicates and solo fallback."""
from crime02_mysql_uat import CFG, run, scalar, begin, pickup
from crime023_mysql_uat import resolve
import mysql.connector


def refresh(db):
    run(db, """INSERT IGNORE INTO crime_competition_rounds
      (contact_id,template_id,hour_bucket,opens_at,expires_at)
      SELECT t.contact_id,t.template_id,FLOOR(UNIX_TIMESTAMP()/3600),
        FLOOR(UNIX_TIMESTAMP()/3600)*3600,
        (FLOOR(UNIX_TIMESTAMP()/3600)+1)*3600
      FROM crime_contract_templates t JOIN crime_contacts n
        ON n.contact_id=t.contact_id AND n.is_enabled=1
      WHERE t.template_id IN(7,8,9) AND t.mode_id=2 AND t.is_enabled=1""")
    run(db, """UPDATE crime_competition_rounds SET state=3,closed_at=UNIX_TIMESTAMP()
      WHERE state=1 AND expires_at<=UNIX_TIMESTAMP()""")
    run(db, """UPDATE crime_contracts c
      JOIN crime_competition_rounds r ON r.round_id=c.competition_id
      SET c.state=4,c.finished_at=UNIX_TIMESTAMP()
      WHERE r.state=3 AND c.state IN(1,2)""")


def accept(db, account, faction, round_id):
    begin(db)
    try:
        n,cid=run(db, """INSERT INTO crime_contracts
          (template_id,account_id,faction_id,mode_id,reward_amount,started_at,
           expires_at,contact_id,scenario_kind,scenario_event,competition_id)
          SELECT t.template_id,m.account_id,m.faction_id,t.mode_id,t.reward_amount,
            UNIX_TIMESTAMP(),r.expires_at,t.contact_id,0,0,r.round_id
          FROM crime_competition_rounds r
          JOIN crime_contract_templates t ON t.template_id=r.template_id
            AND t.mode_id=2 AND t.is_enabled=1
          JOIN faction_members m ON m.account_id=%s AND m.faction_id=%s
          JOIN crime_contact_progress p ON p.account_id=m.account_id
            AND p.contact_id=r.contact_id AND p.trust>=t.required_trust AND p.heat<70
          JOIN crime_syndicate_contacts sc ON sc.faction_id=m.faction_id
            AND sc.contact_id=r.contact_id AND sc.standing>=5
          WHERE r.round_id=%s AND r.state=1
            AND r.expires_at>UNIX_TIMESTAMP()+300
            AND NOT EXISTS(SELECT 1 FROM crime_contracts h
                WHERE h.competition_id=r.round_id AND h.faction_id=m.faction_id)
        """,(account,faction,round_id))
        if n!=1:
            run(db,"ROLLBACK")
            return None
        run(db, """INSERT INTO crime_contract_events
            (contract_id,actor_account_id,event_type) VALUES(%s,%s,'ACCEPTED')""",
            (cid,account))
        run(db,"COMMIT")
        return cid
    except mysql.connector.IntegrityError as ex:
        assert ex.errno==1062,ex
        run(db,"ROLLBACK")
        return None
    except Exception:
        run(db,"ROLLBACK")
        raise


def main():
    db=mysql.connector.connect(**CFG)
    try:
        a=scalar(db,"SELECT account_id FROM accounts WHERE username='CrimeSoloA'")
        b=scalar(db,"SELECT account_id FROM accounts WHERE username='CrimeSoloB'")
        assert a and b
        assert scalar(db,"SELECT COUNT(*) FROM schema_migrations WHERE version=41")==1
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_templates WHERE mode_id=2")==3
        # CRIME-02.1 deliberately leaves CrimeSoloB with an open courier
        # in its isolated fixture; close that test-only job before racing.
        run(db, """UPDATE crime_contracts SET state=5,finished_at=UNIX_TIMESTAMP()
            WHERE account_id=%s AND competition_id IS NULL AND state IN(1,2)""",
            (b,))
        assert scalar(db,"SELECT COUNT(*) FROM crime_contracts WHERE account_id=%s AND state IN(1,2)",(b,))==0
        # Established trust and contacts are durable, but the isolated UAT
        # grants fixtures the required 5 reputation/standing directly.
        for acc,cid in ((a,2),(a,3),(b,2),(b,1)):
            run(db, """INSERT INTO crime_contact_progress
              (account_id,contact_id,trust,completed_count,first_met_at,last_met_at)
              VALUES(%s,%s,10,0,UNIX_TIMESTAMP(),UNIX_TIMESTAMP())
              ON DUPLICATE KEY UPDATE trust=GREATEST(trust,10),heat=0""",
              (acc,cid))
        for faction,cid in ((6,2),(6,3),(7,2),(7,1)):
            run(db, """UPDATE crime_syndicate_contacts SET standing=GREATEST(standing,5)
                WHERE faction_id=%s AND contact_id=%s""",(faction,cid))
        refresh(db)
        assert scalar(db,"SELECT COUNT(*) FROM crime_competition_rounds WHERE hour_bucket=FLOOR(UNIX_TIMESTAMP()/3600)")==3
        # Force sufficient time in the disposable fixture, even near hour end.
        run(db, """UPDATE crime_competition_rounds SET expires_at=UNIX_TIMESTAMP()+1800
          WHERE hour_bucket=FLOOR(UNIX_TIMESTAMP()/3600) AND state=1""")
        rid=scalar(db, """SELECT round_id FROM crime_competition_rounds WHERE contact_id=2
          AND hour_bucket=FLOOR(UNIX_TIMESTAMP()/3600)""")
        assert rid
        ca=accept(db,a,6,rid)
        assert ca
        assert accept(db,a,6,rid) is None  # Only one active account.
        cb=accept(db,b,7,rid)
        assert cb and ca!=cb
        assert scalar(db,"SELECT COUNT(*) FROM crime_contracts WHERE competition_id=%s",(rid,))==2
        before_b=scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(b,))
        before_a=scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))
        other_inf=scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=7")
        pickup(db,ca,a,6)
        pickup(db,cb,b,7)
        assert resolve(db,ca,a,6,0)
        assert not resolve(db,ca,a,6,0)
        assert not resolve(db,cb,b,7,0)  # Loser cannot receive anything.
        assert scalar(db,"SELECT state FROM crime_competition_rounds WHERE round_id=%s",(rid,))==2
        assert scalar(db,"SELECT winner_contract_id FROM crime_competition_rounds WHERE round_id=%s",(rid,))==ca
        assert scalar(db,"SELECT winner_faction_id FROM crime_competition_rounds WHERE round_id=%s",(rid,))==6
        assert scalar(db,"SELECT state FROM crime_contracts WHERE contract_id=%s",(cb,))==5
        assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(b,))==before_b
        assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))==before_a+3400
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards WHERE contract_id=%s",(ca,))==1
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards WHERE contract_id=%s",(cb,))==0
        assert scalar(db,"SELECT influence_delta FROM crime_syndicate_influence_events WHERE contract_id=%s",(ca,))>=8
        assert scalar(db,"SELECT standing_delta FROM crime_syndicate_influence_events WHERE contract_id=%s",(ca,))>=5
        assert scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=7")==other_inf
        assert accept(db,b,7,rid) is None

        # Solo fallback: another shared board, but no competing faction.
        lone=scalar(db,"""SELECT round_id FROM crime_competition_rounds WHERE contact_id=3
          AND hour_bucket=FLOOR(UNIX_TIMESTAMP()/3600)""")
        assert lone
        only=accept(db,a,6,lone)
        assert only
        pickup(db,only,a,6)
        assert resolve(db,only,a,6,0)
        assert scalar(db,"SELECT winner_faction_id FROM crime_competition_rounds WHERE round_id=%s",(lone,))==6
        assert scalar(db,"SELECT COUNT(*) FROM crime_contracts WHERE competition_id=%s",(lone,))==1
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards WHERE contract_id=%s",(only,))==1

        # Closed board can expire and release the remaining account slot.
        exp=scalar(db, """SELECT round_id FROM crime_competition_rounds WHERE contact_id=1
          AND hour_bucket=FLOOR(UNIX_TIMESTAMP()/3600)""")
        ec=accept(db,b,7,exp)
        assert ec
        run(db, """UPDATE crime_contracts SET started_at=UNIX_TIMESTAMP()-3700,
          expires_at=UNIX_TIMESTAMP()-1 WHERE contract_id=%s""",(ec,))
        run(db, """UPDATE crime_competition_rounds SET opens_at=UNIX_TIMESTAMP()-3700,
          expires_at=UNIX_TIMESTAMP()-1 WHERE round_id=%s""",(exp,))
        refresh(db)
        assert scalar(db,"SELECT state FROM crime_competition_rounds WHERE round_id=%s",(exp,))==3
        assert scalar(db,"SELECT state FROM crime_contracts WHERE contract_id=%s",(ec,))==4
        assert not resolve(db,ec,b,7,0)
        print("CRIME-03.1 MariaDB UAT: PASS (two rival syndicates, one winner, solo fallback, expiry, no double payout)")
    finally:
        db.close()


if __name__=="__main__":
    main()
