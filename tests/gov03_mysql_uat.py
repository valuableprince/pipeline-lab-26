"""GOV-03 real MariaDB/UAT, isolated disposable CI database. No production use."""
import concurrent.futures
from datetime import datetime, timedelta, timezone
import os
import time
import mysql.connector

DB=dict(host=os.environ.get("GOV00_TEST_HOST","127.0.0.1"),
        port=int(os.environ.get("GOV00_TEST_PORT","3306")),
        user=os.environ.get("GOV00_TEST_USER","root"),
        password=os.environ["GOV00_TEST_PASSWORD"],
        database=os.environ.get("GOV00_TEST_DB","tarkenso_gov00"),
        autocommit=True)


def query(sql,params=()):
    with mysql.connector.connect(**DB) as db, db.cursor() as cur:
        cur.execute(sql,params)
        return cur.fetchall() if cur.with_rows else cur.rowcount


def call(name,*args):
    with mysql.connector.connect(**DB) as db, db.cursor() as cur:
        cur.execute("CALL "+name+"("+",".join(["%s"]*len(args))+")",args)
        result=cur.fetchone()
        while cur.nextset():
            pass
        return result


def fails(name,*args):
    try:
        call(name,*args)
    except mysql.connector.Error:
        return
    raise AssertionError("Invalid operation accepted "+name+" "+repr(args))


def money():
    return tuple(map(int,query(
        "SELECT balance,bond_fund_reserved,bond_fund_committed,"
        "bond_payout_reserved,minimum_reserve FROM treasury_accounts WHERE code='state'")[0]))


def acct(name,balance):
    query("INSERT INTO accounts(username,password_hash,background,balance) "
          "VALUES(%s,%s,1,%s)",(name,"$2b$12$"+"x"*53,balance))
    return int(query("SELECT account_id FROM accounts WHERE username=%s",(name,))[0][0])


def account(a):
    return tuple(map(int,query(
        "SELECT balance,row_version,txn_serial FROM accounts WHERE account_id=%s",(a,))[0]))


def sleep_until(utc):
    while datetime.now(timezone.utc)<utc:
        time.sleep(0.3)


def main():
    assert query("SELECT version FROM schema_migrations WHERE version=21")==[(21,)]
    creator=int(query("SELECT account_id FROM accounts WHERE username='gov00uat'")[0][0])
    approver=int(query("SELECT account_id FROM accounts WHERE username='gov01uat'")[0][0])
    buyer1=acct("gov03buyer1",35000)
    buyer2=acct("gov03buyer2",35000)
    assert money()[3]==0
    # Restore desired policy for this staging-only issue, with a legal 60s grace.
    query("UPDATE treasury_accounts SET bond_fund_coverage_bps=2000,"
          "bond_fund_premium_bps=200,bond_fund_grace_seconds=60,"
          "bond_fund_enabled=1,bond_fund_config_version=bond_fund_config_version+1 "
          "WHERE code='state'")
    call("treasury_post","gov03:capital",100000,"uat",None,"staging_seed")
    call("bond_fund_capitalize","fund:gov03:capital",100000,None)
    pre=money()
    assert pre[1]>=250000 and pre[2]>=105078,pre

    now=datetime.now(timezone.utc).replace(microsecond=0)
    start=(now-timedelta(minutes=2)).strftime("%Y-%m-%d %H:%M:%S")
    end_time=now+timedelta(seconds=12)
    due_time=now+timedelta(seconds=16)
    end=end_time.strftime("%Y-%m-%d %H:%M:%S")
    due=due_time.strftime("%Y-%m-%d %H:%M:%S")
    issue=call("gov_bond_issue_create","bond:gov03:create","GOV03-A",
        "Investor protection test","Public budget financing","GENERAL",
        10000,10,100,start,end,due,creator)
    assert issue[1:] == ("DRAFT",0),issue
    approve=call("gov_bond_issue_approve","bond:gov03:approve","GOV03-A",approver)
    assert approve[2]==20000,approve
    assert call("gov_bond_issue_open","bond:gov03:open","GOV03-A",approver)[1]=="OFFERING"
    issue_id=int(issue[0])

    # Buying real issues: all balances and ownership in ONE transaction.
    a=call("gov_bond_buy","bond:gov03:buy1","GOV03-A",buyer1,2,0)
    assert a[1]==15000 and a[2]==1 and a[3]==1 and a[-1]==0,a
    assert call("gov_bond_buy","bond:gov03:buy1","GOV03-A",buyer1,2,0)[-1]==1
    fails("gov_bond_buy","bond:gov03:buy1","GOV03-A",buyer1,1,0)
    fails("gov_bond_buy","bond:gov03:stale","GOV03-A",buyer1,1,0)
    fails("gov_bond_buy","bond:gov03:concentration","GOV03-A",buyer1,1,1)
    b=call("gov_bond_buy","bond:gov03:buy2","GOV03-A",buyer2,2,0)
    assert b[1]==15000,b
    assert query("SELECT sold_quantity FROM government_bond_issues "
                 "WHERE issue_id=%s",(issue_id,))==[(4,)]
    assert query("SELECT SUM(quantity) FROM government_bond_holdings "
                 "WHERE issue_id=%s",(issue_id,))==[(4,)]
    assert query("SELECT SUM(amount_paid),SUM(fund_premium) FROM "
                 "government_bond_orders WHERE issue_id=%s",(issue_id,))==[(40000,800)]
    assert query("SELECT COUNT(*) FROM treasury_ledger "
                 "WHERE event_key='bond:gov03:buy1'")==[(1,)]
    assert query("SELECT COUNT(*) FROM bond_fund_ledger "
                 "WHERE event_key='bond:gov03:buy1'")==[(1,)]

    # Spend unreserved budget so only $10k remains for $40k principal + interest.
    available=money()[0]-money()[1]-money()[3]-money()[4]
    assert available>10000,available
    call("treasury_post","gov03:spend",-int(available-10000),
         "uat",None,"discretionary_spending")
    assert money()[0]-money()[1]-money()[3]-money()[4]==10000
    fails("gov_bond_maturity_settle","bond:gov03:maturity","GOV03-A")
    sleep_until(due_time+timedelta(seconds=1))
    maturity=call("gov_bond_maturity_settle","bond:gov03:maturity","GOV03-A")
    assert maturity[1]==10000,maturity
    assert money()[3]==10000
    assert call("gov_bond_maturity_settle","bond:gov03:maturity","GOV03-A")[-1]==1
    fails("gov_bond_maturity_settle","bond:gov03:maturity2","GOV03-A")
    fails("gov_bond_buy","bond:gov03:after","GOV03-A",buyer2,1,1)
    claims=query("SELECT claim_id,account_id,principal_due,interest_due,"
                 "principal_budget_awarded,insurance_awarded,amount_collected "
                 "FROM government_bond_claims WHERE issue_id=%s ORDER BY account_id",
                 (issue_id,))
    assert len(claims)==2,claims
    assert all(c[2:]==(20000,200,5000,0,0) for c in claims),claims
    assert money()[0]>=money()[1]+money()[3]
    fails("treasury_post","gov03:steal",-1,"uat",None,"steal_escrow")
    fails("gov_bond_insurance_assess","bond:gov03:insurance","GOV03-A")

    # Approvals survived offline. Escrow can be collected on connection,
    # while GOV-03 never edits an offline player's wallet directly.
    claim1=int(claims[0][0]); owner1=int(claims[0][1])
    collected=call("gov_bond_claim_collect","bond:gov03:claim1",
                   claim1,owner1,5000,1)
    assert collected[1]==20000 and collected[2]==2,collected
    assert call("gov_bond_claim_collect","bond:gov03:claim1",
                claim1,owner1,5000,1)[-1]==1
    fails("gov_bond_claim_collect","bond:gov03:claim1",claim1,owner1,4999,2)
    fails("gov_bond_claim_collect","bond:gov03:wrong-owner",
          claim1,buyer2,1,1)
    fails("gov_bond_claim_collect","bond:gov03:overclaim",
          claim1,owner1,1,2)
    assert money()[3]==5000

    sleep_until(due_time+timedelta(seconds=61))
    insurance=call("gov_bond_insurance_assess","bond:gov03:insurance","GOV03-A")
    assert insurance[1]==6000,insurance
    assert call("gov_bond_insurance_assess","bond:gov03:insurance","GOV03-A")[-1]==1
    fails("gov_bond_insurance_assess","bond:gov03:insurance-new","GOV03-A")
    assert money()[3]==11000, money()
    assert query("SELECT SUM(insurance_awarded) FROM government_bond_claims "
                 "WHERE issue_id=%s",(issue_id,))==[(6000,)]
    assert query("SELECT debt_status FROM government_bond_issues "
                 "WHERE issue_id=%s",(issue_id,))==[("DEFAULTED",)]
    assert query("SELECT released_exposure,max_exposure FROM "
                 "bond_fund_guarantees WHERE issue_code='GOV03-A'")==[(20000,20000)]
    assert money()[2]==pre[2],money()
    insuranceReceipt=call("gov_bond_claim_collect","bond:gov03:claim1:insurance",
                          claim1,owner1,3000,2)
    assert insuranceReceipt[1]==23000
    claim2=int(claims[1][0]);owner2=int(claims[1][1])
    receipt2=call("gov_bond_claim_collect","bond:gov03:claim2",
                  claim2,owner2,8000,1)
    assert receipt2[1]==23000,receipt2
    assert money()[3]==0
    assert query("SELECT SUM(amount_collected) FROM government_bond_claims "
                 "WHERE issue_id=%s",(issue_id,))==[(16000,)]
    assert query("SELECT COUNT(*) FROM government_bond_claim_receipts")==[(3,)]
    assert query("SELECT COUNT(*) FROM treasury_ledger "
                 "WHERE source='bond_collect'")==[(3,)]
    assert query("SELECT SUM(reserved_delta) FROM bond_payout_ledger "
                 "WHERE issue_id=%s",(issue_id,))==[(0,)]

    print("GOV-03 MariaDB UAT: PASS (real purchases, idempotent settlement, "
          "partial pro-rata, offline escrows, 20% insurance, exact receipt delivery)")


if __name__=="__main__":
    main()
