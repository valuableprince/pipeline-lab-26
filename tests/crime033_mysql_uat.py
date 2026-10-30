"""CRIME-03.3: real DB gate, persistence, risk, one-time NPC choices."""
import mysql.connector
from crime02_mysql_uat import CFG, run, scalar, pickup
from crime023_mysql_uat import resolve
from crime032_mysql_uat import bid, close, finalize, incident

def make_round(db,contact,bucket_offset=-3):
    tid=scalar(db,"SELECT template_id FROM crime_contract_templates WHERE mode_id=3 AND contact_id=%s",(contact,))
    assert tid
    n,rid=run(db, """INSERT INTO crime_tender_rounds
       (contact_id,template_id,bucket,opens_at,closes_at)
       VALUES(%s,%s,FLOOR(UNIX_TIMESTAMP()/600)+%s,
          UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+60)""",
        (contact,tid,bucket_offset))
    assert n==1
    return rid

def main():
    db=mysql.connector.connect(**CFG)
    try:
        a=scalar(db,"SELECT account_id FROM accounts WHERE username='CrimeSoloA'")
        assert a
        assert scalar(db,"SELECT COUNT(*) FROM schema_migrations WHERE version=43")==1
        # Previously won tenders must have a single durable encounter.
        assert scalar(db,"SELECT COUNT(*) FROM crime_contracts WHERE mode_id=3")>=3
        assert scalar(db, """SELECT COUNT(*) FROM crime_contracts c
          LEFT JOIN crime_tender_incidents i ON i.contract_id=c.contract_id
          WHERE c.mode_id=3 AND c.state IN(1,2) AND i.contract_id IS NULL""")==0

        rid=make_round(db,2)
        assert bid(db,rid,a,6,2)
        close(db,rid)
        cid=finalize(db,rid)
        assert cid
        assert scalar(db,"SELECT COUNT(*) FROM crime_tender_incidents WHERE contract_id=%s",(cid,))==1
        assert scalar(db,"SELECT incident_code FROM crime_tender_incidents WHERE contract_id=%s",(cid,))==1+cid%3
        assert scalar(db,"SELECT choice FROM crime_tender_incidents WHERE contract_id=%s",(cid,))==0
        base=scalar(db,"SELECT reward_amount FROM crime_contracts WHERE contract_id=%s",(cid,))
        before=scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))
        pickup(db,cid,a,6)
        assert not resolve(db,cid,a,6,0)  # The delivery may NOT skip the NPC.
        assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))==before
        assert not scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards WHERE contract_id=%s",(cid,))
        assert incident(db,cid,a,6,2)
        assert not incident(db,cid,a,6,1)  # Once only; choice cannot be overwritten.
        assert scalar(db,"SELECT reward_amount FROM crime_contracts WHERE contract_id=%s",(cid,))==base+300
        assert scalar(db,"SELECT choice FROM crime_tender_incidents WHERE contract_id=%s",(cid,))==2
        assert scalar(db,"SELECT resolved_at FROM crime_tender_incidents WHERE contract_id=%s",(cid,))>0
        assert scalar(db, """SELECT COUNT(*) FROM crime_contract_events
             WHERE contract_id=%s AND event_type='INCIDENT_RESOLVED'""",(cid,))==1
        assert resolve(db,cid,a,6,0)
        assert not resolve(db,cid,a,6,0)
        assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))==before+base+300
        assert scalar(db,"SELECT risk_delta FROM crime_contracts WHERE contract_id=%s",(cid,))==12
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards WHERE contract_id=%s",(cid,))==1

        # Separate solo tender: choosing caution must NOT create extra money.
        rid2=make_round(db,3)
        assert bid(db,rid2,a,6,3)
        close(db,rid2)
        careful=finalize(db,rid2)
        assert careful and careful!=cid
        base2=scalar(db,"SELECT reward_amount FROM crime_contracts WHERE contract_id=%s",(careful,))
        before2=scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))
        pickup(db,careful,a,6)
        assert not resolve(db,careful,a,6,0)
        assert incident(db,careful,a,6,1)
        assert not incident(db,careful,a,6,2)
        assert scalar(db,"SELECT reward_amount FROM crime_contracts WHERE contract_id=%s",(careful,))==base2
        assert resolve(db,careful,a,6,0)
        assert scalar(db,"SELECT risk_delta FROM crime_contracts WHERE contract_id=%s",(careful,))==-12
        assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))==before2+base2

        # Existing race and solo contracts remain incident-free.
        assert scalar(db, """SELECT COUNT(*) FROM crime_tender_incidents i
          JOIN crime_contracts c ON c.contract_id=i.contract_id
          WHERE c.mode_id<>3""")==0
        print("CRIME-03.3 MariaDB UAT: PASS (required NPC, safe/risky, +300, heat, replay, solo)")
    finally:
        db.close()

if __name__=="__main__":
    main()
