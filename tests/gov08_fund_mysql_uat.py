"""GOV-08: minister fund-capitalization on disposable MariaDB, after 029.
Financial invariants: no cash minted, append-only ledger, one event per top-up,
minister authorization under SQL, no duplicates under replay or invalid inputs.
"""
import os
import mysql.connector

DB_NAME=os.environ.get("GOV00_TEST_DB","tarkenso_gov00")
assert DB_NAME=="tarkenso_gov00", "Refuse non-disposable database"
CFG=dict(host=os.environ.get("GOV00_TEST_HOST","127.0.0.1"),
         port=int(os.environ.get("GOV00_TEST_PORT","3306")),
         user=os.environ.get("GOV00_TEST_USER","root"),
         password=os.environ["GOV00_TEST_PASSWORD"],
         database=DB_NAME,autocommit=True)

def sql(statement, params=()):
    with mysql.connector.connect(**CFG) as db,db.cursor() as c:
        c.execute(statement,params)
        return c.fetchall() if c.with_rows else c.rowcount

def call(procedure,*args):
    with mysql.connector.connect(**CFG) as db, db.cursor() as c:
        c.execute("CALL "+procedure+"("+",".join(["%s"]*len(args))+")",args)
        row=c.fetchone()
        while c.nextset():pass
        return row

def deny(procedure,*args):
    try: call(procedure,*args)
    except mysql.connector.Error: return
    raise AssertionError("Unauthorized or unsafe fund operation accepted: "+repr(args))

def snapshot():
    return sql("SELECT balance,bond_fund_reserved,bond_fund_committed,"
               "row_version FROM treasury_accounts WHERE code='state'")[0]

def main():
    assert sql("SELECT version FROM schema_migrations WHERE version=29")==[(29,)]
    minister=sql("SELECT account_id FROM accounts WHERE username='gov03buyer1'")[0][0]
    deputy=sql("SELECT account_id FROM accounts WHERE username='gov01uat'")[0][0]

    # Disposable fixture only. Set explicit valid term to avoid expiry.
    sql("UPDATE factions SET is_enabled=1,term_end=UNIX_TIMESTAMP()+86400 "
        "WHERE faction_id=5")
    sql("UPDATE faction_members SET rank_id=4 WHERE faction_id=5 AND account_id=%s",
        (minister,))
    sql("UPDATE faction_members SET rank_id=3 WHERE faction_id=5 AND account_id=%s",
        (deputy,))

    if snapshot()[0]-sql("SELECT minimum_reserve+bond_fund_reserved "
                         "FROM treasury_accounts WHERE code='state'")[0][0]<1000:
        call("treasury_post","treasury:gov08:seed",10000,"admin",None,"GOV-08 UAT seed")

    before=snapshot()
    assert before[0]>=before[1]+1000
    key="fund:gov5:uat:approved"
    deny("gov_bond_official_fund_capitalize","fund:gov5:uat:deputy",deputy,200)
    deny("gov_bond_official_fund_capitalize","fund:gov5:uat:unknown",999999999,200)
    deny("gov_bond_official_fund_capitalize","fund:invalid",minister,200)
    deny("gov_bond_official_fund_capitalize","fund:gov5:uat:zero",minister,0)
    assert snapshot()==before
    assert sql("SELECT COUNT(*) FROM bond_fund_ledger WHERE event_key LIKE "
               "'fund:gov5:uat:%'")==[(0,)]

    receipt=call("gov_bond_official_fund_capitalize",key,minister,200)
    cash,reserve,committed,version,amount,actor,replayed=receipt
    assert (cash,reserve,committed,version)==(
        before[0],before[1]+200,before[2],before[3]+1),receipt
    assert (amount,actor,replayed)==(200,minister,0),receipt
    assert snapshot()==(before[0],before[1]+200,before[2],before[3]+1)
    row=sql("SELECT operation,reserve_delta,commitment_delta,actor_account_id "
            "FROM bond_fund_ledger WHERE event_key=%s",(key,))
    assert row==[("CAPITALIZE",200,0,minister)],row
    assert call("gov_bond_official_fund_capitalize",key,minister,200)[-1]==1
    assert snapshot()==(before[0],before[1]+200,before[2],before[3]+1)
    deny("gov_bond_official_fund_capitalize",key,minister,201)
    deny("gov_bond_official_fund_capitalize",key,deputy,200)

    free=sql("SELECT balance-minimum_reserve-bond_fund_reserved,"
             "max_transaction FROM treasury_accounts WHERE code='state'")[0]
    deny("gov_bond_official_fund_capitalize","fund:gov5:uat:overdraft",
         minister,min(1000000000,int(free[0])+1))
    deny("gov_bond_official_fund_capitalize","fund:gov5:uat:too_big",
         minister,1000000001)
    assert sql("SELECT COUNT(*) FROM bond_fund_ledger WHERE event_key=%s",
               ("fund:gov5:uat:overdraft",))==[(0,)]

    sql("UPDATE faction_members SET rank_id=2 WHERE faction_id=5 AND account_id=%s",
        (minister,))
    deny("gov_bond_official_fund_capitalize","fund:gov5:uat:revoked",minister,10)
    deny("gov_bond_official_fund_capitalize",key,minister,200)
    sql("UPDATE faction_members SET rank_id=4 WHERE faction_id=5 AND account_id=%s",
        (minister,))
    assert snapshot()==(before[0],before[1]+200,before[2],before[3]+1)
    assert sql("SELECT COUNT(*) FROM bond_fund_ledger WHERE event_key LIKE "
               "'fund:gov5:uat:%'")==[(1,)]
    assert call("gov_finance_assert_integrity")[0]=="PASS"
    print("GOV-08 PASS: minister authorization, free-budget cap, ledger, replay, "
          "revocation, intact treasury and financial integrity")

if __name__=="__main__":
    main()
