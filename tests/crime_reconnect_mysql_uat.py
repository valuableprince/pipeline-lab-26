"""CRIME-UAT-01: reconnect notifications against real durable MariaDB state.

Uses the exact read queries from crime_reconnect.inc. All other statements
prepare isolated CI-only fixtures; no production accounts are accessed.
"""
import re
import time
from pathlib import Path
import mysql.connector
from crime02_mysql_uat import CFG, run, scalar
from crime032_mysql_uat import finalize

SRC=(Path(__file__).resolve().parents[1] /
     "gamemodes/includes/crime_reconnect.inc").read_text(encoding="utf-8")


def production_sql(prefix):
    matches=re.findall(r'"(SELECT [^"]+)"',SRC)
    sql=[x for x in matches if x.startswith(prefix)]
    assert len(sql)==1,(prefix,len(sql))
    return sql[0].replace("%d","%s")


CONTRACT=production_sql("SELECT c.contract_id,c.state,c.faction_id,c.expires_at,c.scenario_kind,t.title")
PENDING=production_sql("SELECT b.bid_id,b.tender_id,r.closes_at")
BID_STATE=production_sql("SELECT state FROM crime_tender_bids WHERE bid_id=")


def read(db,sql,params=()):
    cur=db.cursor()
    try:
        cur.execute(sql,params)
        return cur.fetchall()
    finally:
        cur.close()


def member(db,username,faction):
    run(db,"INSERT INTO accounts(username,password_hash,background) VALUES(%s,%s,1)",
        (username,"$2b$12$"+"u"*53))
    account=scalar(db,"SELECT account_id FROM accounts WHERE username=%s",(username,))
    run(db,"INSERT INTO faction_members(account_id,faction_id,rank_id,joined_at,last_seen) VALUES(%s,%s,1,100,100)",
        (account,faction))
    run(db,"INSERT INTO crime_contact_progress(account_id,contact_id,trust,heat,first_met_at,last_met_at) VALUES(%s,1,10,0,UNIX_TIMESTAMP(),UNIX_TIMESTAMP())",(account,))
    run(db,"UPDATE crime_syndicate_contacts SET standing=GREATEST(standing,5) WHERE faction_id=%s AND contact_id=1",(faction,))
    return account


def tender(db,bucket):
    # Unique bucket is a fixture only: tender finalization uses closes_at
    # and the same DB account/faction gates as production.
    count,tender_id=run(db,"""INSERT INTO crime_tender_rounds
        (contact_id,template_id,bucket,opens_at,closes_at)
        VALUES(1,10,%s,UNIX_TIMESTAMP()-20,UNIX_TIMESTAMP()+40)""",(bucket,))
    assert count==1 and tender_id>0
    return tender_id


def offer(db,tid,account,faction,code):
    n,bid=run(db,"""INSERT INTO crime_tender_bids
        (tender_id,account_id,faction_id,offer_code,submitted_at)
        VALUES(%s,%s,%s,%s,UNIX_TIMESTAMP())""",(tid,account,faction,code))
    assert n==1
    return bid


def close(db,tid):
    assert run(db,"UPDATE crime_tender_rounds SET closes_at=UNIX_TIMESTAMP()-1 WHERE tender_id=%s AND state=1",(tid,))[0]==1


def main():
    db=mysql.connector.connect(**CFG)
    try:
        assert scalar(db,"SELECT COUNT(*) FROM schema_migrations WHERE version=46")==1
        account=member(db,"CrimeReconnectCI",6)
        rival=member(db,"CrimeRivalReconnect",7)
        before_cash=scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(account,))
        assert read(db,CONTRACT,(account,))==[]
        assert read(db,PENDING,(account,6))==[]

        # Active stage 1 persisted independently from the Pawn player slot.
        n,cid=run(db,"""INSERT INTO crime_contracts(template_id,account_id,
            faction_id,mode_id,state,reward_amount,started_at,expires_at)
            VALUES(1,%s,6,1,1,2000,UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+1800)""",(account,))
        assert n==1
        first=read(db,CONTRACT,(account,))
        assert len(first)==1 and first[0][0]==cid and first[0][1]==1
        assert first[0][3]>int(time.time())
        run(db,"UPDATE crime_contracts SET state=2,picked_up_at=UNIX_TIMESTAMP() WHERE contract_id=%s",(cid,))
        # Independent connection simulates server restart / player relog.
        other=mysql.connector.connect(**CFG)
        try:
            second=read(other,CONTRACT,(account,))
            assert len(second)==1 and second[0][0]==cid and second[0][1]==2
            assert second[0][5]=="Ночной рейс"
        finally:
            other.close()

        # Time continues offline. The production expiry ledger is replay-safe.
        run(db,"UPDATE crime_contracts SET started_at=UNIX_TIMESTAMP()-2000,expires_at=UNIX_TIMESTAMP()-1 WHERE contract_id=%s",(cid,))
        assert read(db,CONTRACT,(account,))[0][3]<int(time.time())
        for _ in range(2):
            run(db,"START TRANSACTION")
            run(db,"""INSERT IGNORE INTO crime_contract_events
               (contract_id,actor_account_id,event_type)
               SELECT contract_id,account_id,'EXPIRED' FROM crime_contracts
               WHERE account_id=%s AND state IN(1,2)
                 AND expires_at<=UNIX_TIMESTAMP()""",(account,))
            run(db,"""UPDATE crime_contracts SET state=4,finished_at=UNIX_TIMESTAMP()
               WHERE account_id=%s AND state IN(1,2)
                 AND expires_at<=UNIX_TIMESTAMP()""",(account,))
            run(db,"COMMIT")
        assert scalar(db,"SELECT state FROM crime_contracts WHERE contract_id=%s",(cid,))==4
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_events WHERE contract_id=%s AND event_type='EXPIRED'",(cid,))==1
        assert read(db,CONTRACT,(account,))==[]
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards WHERE contract_id=%s",(cid,))==0
        assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(account,))==before_cash

        # Pending NPC offer: the chat may report 'awaiting decision' only
        # while b.state=1 AND r.state=1, with a real future close.
        bucket=int(time.time())+90000
        tid=tender(db,bucket)
        losing=offer(db,tid,account,6,3)
        offer(db,tid,rival,7,1)
        waiting=read(db,PENDING,(account,6))
        assert len(waiting)==1 and waiting[0][:2]==(losing,tid)
        assert waiting[0][2]>int(time.time())
        close(db,tid)
        assert finalize(db,tid)
        assert read(db,PENDING,(account,6))==[]
        assert read(db,BID_STATE,(losing,account,6))==[(3,)]
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards WHERE account_id=%s",(account,))==0

        # Solo NPC award after logout: no other faction online is required.
        solo=tender(db,bucket+1)
        winning=offer(db,solo,account,6,2)
        assert read(db,PENDING,(account,6))[0][:2]==(winning,solo)
        close(db,solo)
        won=finalize(db,solo)
        assert won
        assert read(db,BID_STATE,(winning,account,6))==[(2,)]
        restored=read(db,CONTRACT,(account,))
        assert len(restored)==1 and restored[0][0]==won and restored[0][1]==1
        assert read(db,PENDING,(account,6))==[]
        assert finalize(db,solo) is None
        assert scalar(db,"SELECT COUNT(*) FROM crime_contracts WHERE tender_id=%s",(solo,))==1
        assert scalar(db,"SELECT COUNT(*) FROM crime_contract_rewards WHERE contract_id=%s",(won,))==0
        assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(account,))==before_cash
        print("CRIME-UAT-01 reconnect MariaDB: PASS (empty, stage-1/2 relog, expiration once, pending NPC, lost bid, solo award, no extra payout)")
    finally:
        db.close()


if __name__=="__main__":
    main()
