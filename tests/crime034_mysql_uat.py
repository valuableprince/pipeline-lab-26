"""CRIME-03.4: branch-specific NPC stage and transactional reputation."""
import mysql.connector
from crime02_mysql_uat import CFG, run, scalar, pickup
from crime023_mysql_uat import resolve
from crime032_mysql_uat import bid, close, finalize, incident, followup
from crime033_mysql_uat import make_round


def check_branch(db, account, contact, choice, expected_trust, expected_standing, offset):
    rid=make_round(db,contact,offset)
    assert bid(db,rid,account,6,2)
    close(db,rid)
    cid=finalize(db,rid)
    assert cid
    assert scalar(db,"SELECT COUNT(*) FROM crime_tender_aftermath WHERE contract_id=%s",(cid,))==0
    reward=scalar(db,"SELECT reward_amount FROM crime_contracts WHERE contract_id=%s",(cid,))
    before=scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(account,))
    pickup(db,cid,account,6)
    assert not resolve(db,cid,account,6,0), "Incident may not be skipped"
    assert incident(db,cid,account,6,choice)
    assert scalar(db,"SELECT state FROM crime_tender_aftermath WHERE contract_id=%s",(cid,))==1
    assert scalar(db,"SELECT choice FROM crime_tender_aftermath WHERE contract_id=%s",(cid,))==choice
    assert scalar(db,"SELECT finished_at FROM crime_tender_aftermath WHERE contract_id=%s",(cid,))==0
    assert not resolve(db,cid,account,6,0), "Follow-up may not be skipped"
    assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(account,))==before
    assert followup(db,cid,account,6)
    assert not followup(db,cid,account,6), "Second completion must fail"
    assert scalar(db,"SELECT state FROM crime_tender_aftermath WHERE contract_id=%s",(cid,))==2
    assert scalar(db,"SELECT finished_at FROM crime_tender_aftermath WHERE contract_id=%s",(cid,))>0
    assert scalar(db, """SELECT COUNT(*) FROM crime_contract_events
        WHERE contract_id=%s AND event_type='AFTERMATH_DONE'""",(cid,))==1
    assert resolve(db,cid,account,6,0)
    assert not resolve(db,cid,account,6,0)
    assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(account,))==before+reward+(300 if choice==2 else 0)
    assert scalar(db,"SELECT trust_delta FROM crime_contact_trust_events WHERE contract_id=%s",(cid,))==expected_trust
    active=scalar(db,"""SELECT COUNT(*) FROM crime_syndicate_assets
        WHERE faction_id=6 AND contact_id=%s AND asset_code=2 AND activated_at>0""",(contact,))
    assert scalar(db,"SELECT standing_delta FROM crime_syndicate_influence_events WHERE contract_id=%s",(cid,))==expected_standing+active
    assert scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards WHERE contract_id=%s",(cid,))==1
    return cid


def main():
    db=mysql.connector.connect(**CFG)
    try:
        a=scalar(db,"SELECT account_id FROM accounts WHERE username='CrimeSoloA'")
        assert a and scalar(db,"SELECT COUNT(*) FROM schema_migrations WHERE version=44")==1
        careful=check_branch(db,a,2,1,7,4,-5)
        risky=check_branch(db,a,3,2,2,1,-6)
        assert careful!=risky
        assert scalar(db,"SELECT COUNT(*) FROM crime_tender_aftermath")==scalar(db,"SELECT COUNT(*) FROM crime_tender_incidents WHERE choice IN(1,2)")
        assert scalar(db, """SELECT COUNT(*) FROM crime_tender_aftermath a
            JOIN crime_contracts c ON c.contract_id=a.contract_id
            WHERE c.mode_id<>3""")==0
        print("CRIME-03.4 MariaDB UAT: PASS (physical branches, payout gate, trust/standing, replay, solo)")
    finally:
        db.close()


if __name__=="__main__":
    main()
