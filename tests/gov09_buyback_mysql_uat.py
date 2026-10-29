"""GOV-09 UAT: run only on the disposable CI database AFTER migration 030.
Uses an existing real purchased bond from GOV-03/07 UAT; never changes production.
"""
import os
import mysql.connector

NAME=os.environ.get("GOV00_TEST_DB","tarkenso_gov00")
assert NAME=="tarkenso_gov00","GOV-09 tests must never run on a persistent DB"
CFG=dict(host=os.environ.get("GOV00_TEST_HOST","127.0.0.1"),
         port=int(os.environ.get("GOV00_TEST_PORT","3306")),
         user=os.environ.get("GOV00_TEST_USER","root"),
         password=os.environ["GOV00_TEST_PASSWORD"], database=NAME,autocommit=True)

def rows(statement,parameters=()):
    with mysql.connector.connect(**CFG) as db,db.cursor() as c:
        c.execute(statement,parameters)
        return c.fetchall()

def call(name,*args):
    with mysql.connector.connect(**CFG) as db,db.cursor() as c:
        c.execute("CALL "+name+"("+",".join(["%s"]*len(args))+")",args)
        result=c.fetchone()
        while c.nextset():pass
        return result

def deny(*args):
    try:call("gov_bond_buyback",*args)
    except mysql.connector.Error:return
    raise AssertionError("Unauthorized/stale bond buyback succeeded: "+str(args))

def main():
    assert rows("SELECT version FROM schema_migrations WHERE version=30")==[(30,)]
    assert call("gov_finance_assert_integrity")[0]=="PASS"
    found=rows("""
      SELECT h.issue_id,h.account_id,h.quantity,i.face_value
      FROM government_bond_holdings h JOIN government_bond_issues i ON i.issue_id=h.issue_id
      WHERE i.maturity_at>UTC_TIMESTAMP()+INTERVAL 1 DAY
        AND i.maturity_event_key IS NULL
        AND i.status IN ('APPROVED','OFFERING','CLOSED')
        AND i.face_value>=100
      ORDER BY h.quantity DESC,h.issue_id LIMIT 1
    """)
    assert found,"GOV-09 needs an active, previously purchased test issue"
    issue,account,owned,face=found[0]
    unit=face*98//100
    price=unit
    assert price>0
    balance,version,serial=rows(
        "SELECT balance,row_version,txn_serial FROM accounts WHERE account_id=%s",
        (account,))[0]
    state=rows("""SELECT balance,bond_fund_reserved,bond_payout_reserved,
                        minimum_reserve,bond_fund_committed,max_transaction
                 FROM treasury_accounts WHERE code='state'""")[0]
    free=state[0]-state[1]-state[2]-state[3]
    assert free>=price and state[5]>=price,(state,price)
    wallet=rows("""SELECT accrued,collected,converted FROM government_bond_coupon_wallets
                   WHERE issue_id=%s AND account_id=%s""",(issue,account))
    before_coupon=wallet[0] if wallet else None
    before_issue=rows("""SELECT bought_back_quantity,sold_quantity FROM government_bond_issues
                         WHERE issue_id=%s""",(issue,))[0]
    before_exposure=rows("""SELECT released_exposure FROM bond_fund_guarantees g
                            JOIN government_bond_issues i ON g.issue_code=i.issue_code
                            WHERE i.issue_id=%s""",(issue,))[0][0]

    key="bond:gov09:uat:one"
    deny("bond:gov09:uat:zero",issue,account,0,version)
    deny("bond:gov09:uat:too_many",issue,account,owned+1,version)
    deny("bond:gov09:uat:other_owner",issue,999999999,1,version)
    assert rows("SELECT COUNT(*) FROM government_bond_buybacks WHERE issue_id=%s",(issue,))==[(0,)]
    result=call("gov_bond_buyback",key,issue,account,1,version)
    assert result[0]==issue and result[1]==price and result[-1]==0,result
    assert rows("SELECT balance,row_version,txn_serial FROM accounts WHERE account_id=%s",
                (account,))==[(balance+price,version+1,serial+1)]
    assert rows("SELECT balance FROM treasury_accounts WHERE code='state'")==[(state[0]-price,)]
    assert rows("SELECT bought_back_quantity,sold_quantity FROM government_bond_issues WHERE issue_id=%s",
                (issue,))==[(before_issue[0]+1,before_issue[1])]
    after_held=rows("SELECT quantity FROM government_bond_holdings WHERE issue_id=%s AND account_id=%s",
                    (issue,account))
    assert (after_held[0][0] if after_held else 0)==owned-1
    audit=rows("""SELECT buyback_id,unit_price,quantity,amount_paid,released_exposure
                  FROM government_bond_buybacks WHERE event_key=%s""",(key,))
    assert len(audit)==1 and audit[0][1:4]==(unit,1,price),audit
    buyback_id,_,_,_,release=audit[0]
    alloc=rows("SELECT SUM(quantity),SUM(frozen_coupon_amount) FROM government_bond_buyback_allocations WHERE buyback_id=%s",
               (buyback_id,))[0]
    assert alloc[0]==1
    assert rows("SELECT amount_delta,source FROM treasury_ledger WHERE event_key=%s",
                (key,))==[(-price,"bond_buyback")]
    assert rows("""SELECT released_exposure FROM bond_fund_guarantees g
                   JOIN government_bond_issues i ON i.issue_code=g.issue_code
                   WHERE i.issue_id=%s""",(issue,))==[(before_exposure+release,)]
    after_coupon=rows("SELECT accrued,collected,converted FROM government_bond_coupon_wallets WHERE issue_id=%s AND account_id=%s",
                      (issue,account))
    if before_coupon:
        assert after_coupon==[before_coupon],"Previously accrued coupons were lost"
    assert call("gov_finance_assert_integrity")[0]=="PASS"

    # Replay must neither pay twice nor rewrite append-only allocations.
    replay=call("gov_bond_buyback",key,issue,account,1,version)
    assert replay[-1]==1
    deny(key,issue,account,2,version)
    deny("bond:gov09:uat:stale",issue,account,1,version)
    assert rows("SELECT COUNT(*) FROM government_bond_buybacks WHERE event_key=%s",
                (key,))==[(1,)]
    assert rows("SELECT balance FROM accounts WHERE account_id=%s",(account,))==[(balance+price,)]
    assert call("gov_finance_assert_integrity")[0]=="PASS"
    # Append-only: untrusted caller cannot delete or rewrite evidence.
    for stmt in ("DELETE FROM government_bond_buybacks WHERE buyback_id=%s",
                 "UPDATE government_bond_buybacks SET amount_paid=1 WHERE buyback_id=%s"):
        try:rows(stmt,(buyback_id,))
        except mysql.connector.Error:continue
        raise AssertionError("Buyback history was mutable")
    print("GOV-09 PASS: 98%% price, protected treasury transfer, ownership, frozen coupons, "
          "insurance release, exact receipt, replay, stale version, immutable audit")

if __name__=="__main__":
    main()
