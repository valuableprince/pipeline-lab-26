"""CRIME-02.7 MariaDB UAT: per-contact heat, risk caps and solo recovery.

Runs after CRIME-02.6 on a disposable CI MariaDB database.
"""
from crime02_mysql_uat import CFG, run, scalar
from crime024_mysql_uat import attempt
from crime025_mysql_uat import collect
from crime023_mysql_uat import first_stage, interview, resolve
import mysql.connector


def age(con, account, template):
    run(con, """UPDATE crime_contracts SET finished_at=UNIX_TIMESTAMP()-4000
        WHERE account_id=%s AND template_id=%s AND state IN(3,5)""",
        (account,template))


def courier(con, account):
    age(con,account,2)
    cid=attempt(con,account,6,2,2)
    assert cid, "courier should remain available irrespective of heat"
    changed,_=run(con, """UPDATE crime_contracts SET state=2,
        picked_up_at=UNIX_TIMESTAMP() WHERE contract_id=%s AND state=1""",(cid,))
    assert changed==1
    assert resolve(con,cid,account,6,0)
    assert not resolve(con,cid,account,6,0)
    return cid


def main():
    con=mysql.connector.connect(**CFG)
    try:
        a=scalar(con,"SELECT account_id FROM accounts WHERE username='CrimeSoloA'")
        assert a
        assert scalar(con,"SELECT COUNT(*) FROM schema_migrations WHERE version=38")==1
        # Pre-038 fixture payouts from CRIME-02.2 are intentionally not
        # retroactively assigned heat, so ledger counts need not match.
        assert scalar(con,"SELECT COUNT(*) FROM crime_contact_heat_events")>0
        # Risk is contact-local. High attention blocks investigations, not couriers.
        run(con,"UPDATE crime_contact_progress SET heat=72 WHERE account_id=%s AND contact_id=2",(a,))
        age(con,a,4)
        assert attempt(con,a,6,4,2) is None
        cid=courier(con,a)
        assert scalar(con,"SELECT heat_delta FROM crime_contact_heat_events WHERE contract_id=%s",(cid,))==-8
        assert scalar(con,"SELECT heat FROM crime_contact_progress WHERE account_id=%s AND contact_id=2",(a,))==64
        assert scalar(con,"SELECT COUNT(*) FROM crime_contract_rewards WHERE contract_id=%s",(cid,))==1

        age(con,a,4)
        cid=attempt(con,a,6,4,2)
        assert cid
        assert collect(con,cid,a,6)
        assert first_stage(con,cid,a,6,1)
        assert interview(con,cid,a,6,2)
        assert resolve(con,cid,a,6,2)  # Concealment: +20 attention.
        assert not resolve(con,cid,a,6,2)
        assert scalar(con,"SELECT risk_delta FROM crime_contracts WHERE contract_id=%s",(cid,))==20
        assert scalar(con,"SELECT heat_delta FROM crime_contact_heat_events WHERE contract_id=%s",(cid,))==20
        assert scalar(con,"SELECT heat FROM crime_contact_progress WHERE account_id=%s AND contact_id=2",(a,))==84
        assert attempt(con,a,6,4,2) is None
        assert scalar(con,"SELECT COUNT(*) FROM crime_contact_heat_events WHERE contract_id=%s",(cid,))==1

        # Solo activity always remains possible: two ordinary deliveries
        # reduce local attention 84 -> 76 -> 68, unlocking investigations.
        for expected in (76,68):
            cid=courier(con,a)
            assert scalar(con,"SELECT heat FROM crime_contact_progress WHERE account_id=%s AND contact_id=2",(a,))==expected
        age(con,a,4)
        unlocked=attempt(con,a,6,4,2)
        assert unlocked
        # Leave a closed fixture without a payout; just exercising eligibility.
        run(con, "UPDATE crime_contracts SET state=5,finished_at=UNIX_TIMESTAMP() WHERE contract_id=%s",(unlocked,))

        # Zero clamp: the ledger records the actual change, not -8 blindly.
        run(con,"UPDATE crime_contact_progress SET heat=0 WHERE account_id=%s AND contact_id=2",(a,))
        cid=courier(con,a)
        assert scalar(con,"SELECT heat FROM crime_contact_progress WHERE account_id=%s AND contact_id=2",(a,))==0
        assert scalar(con,"SELECT heat_delta FROM crime_contact_heat_events WHERE contract_id=%s",(cid,))==0
        assert scalar(con,"SELECT COUNT(*) FROM crime_contact_heat_events WHERE contract_id=%s",(cid,))==1
        assert scalar(con,"SELECT heat FROM crime_contact_progress WHERE account_id=%s AND contact_id=1",(a,)) is None
        print("CRIME-02.7 MariaDB UAT: PASS (risk gate, courier recovery, atomic ledgers, replay, zero clamp)")
    finally:
        con.close()


if __name__=="__main__":
    main()
