"""GOV-06 disposable MariaDB security and whole-finance integrity acceptance.

Checks negative authority, tamper rejection, live reconciliation after GOV-00—05,
and multithreaded read-only audit. No business migrations on production.
"""
from concurrent.futures import ThreadPoolExecutor
import os
import time
import mysql.connector

DB=dict(host=os.environ.get("GOV00_TEST_HOST","127.0.0.1"),
        port=int(os.environ.get("GOV00_TEST_PORT","3306")),
        user=os.environ.get("GOV00_TEST_USER","root"),
        password=os.environ["GOV00_TEST_PASSWORD"],
        database=os.environ.get("GOV00_TEST_DB","tarkenso_gov00"),
        autocommit=True)

def q(sql,args=(),credentials=None):
    with mysql.connector.connect(**(credentials or DB)) as db,db.cursor() as cur:
        cur.execute(sql,args)
        row=cur.fetchall() if cur.with_rows else None
        while cur.nextset(): pass
        return row

def deny(sql,args=(),credentials=None):
    try: q(sql,args,credentials)
    except mysql.connector.Error: return
    raise AssertionError("Forbidden SQL operation accepted: "+sql)

def call_audit(credentials=None):
    r=q("CALL gov_finance_assert_integrity()",(),credentials)
    assert r and r[0][0]=="PASS",r
    return r[0]

def main():
    dbname=DB["database"]
    assert q("SELECT version FROM schema_migrations WHERE version=24")==[(24,)]
    baseline=call_audit()
    assert baseline[1]>=baseline[2]+baseline[4]
    assert baseline[3]<=baseline[2]
    assert baseline[7]>=9

    # No app user should be able to mutate immutable proof rows, even if it has
    # inherited table DML privileges. The triggers guard the finance journals.
    immutable={
        "treasury_ledger":"note",
        "bond_fund_ledger":"reference_code",
        "bond_payout_ledger":"event_key",
        "government_bond_issue_audit":"action",
        "government_bond_orders":"event_key",
        "government_bond_claim_receipts":"event_key",
        "government_bond_recoveries":"event_key",
        "government_bond_restructure_audit":"event_key",
        "government_bond_role_audit":"event_key",
        "bond_crisis_events":"event_key",
        "government_bond_tax_earmarks":"fund_event_key",
    }
    for table,col in immutable.items():
        # Even a no-op update is forbidden; no audit history rewriting.
        deny(f"UPDATE {table} SET {col}={col}")
        deny(f"DELETE FROM {table} WHERE 1=0")

    procs={r[0]:r[1] for r in q(
        "SELECT ROUTINE_NAME,SECURITY_TYPE FROM information_schema.ROUTINES "
        "WHERE ROUTINE_SCHEMA=%s AND ROUTINE_TYPE='PROCEDURE'",(dbname,))}
    for name in ("treasury_post","gov_transfer_post","gov_bond_buy",
        "gov_bond_issue_approve","gov_bond_claim_collect","bond_fund_capitalize",
        "gov_bond_issue_create_auto","gov_bond_role_set",
        "gov_bond_recovery_allocate","gov_bond_tax_earmark",
        "gov_finance_assert_integrity"):
        assert procs.get(name)=="DEFINER",(name,procs.get(name))

    # A schema/account with READ+audit EXECUTE only must not be able
    # to call any money-moving or role-grant routine or write financial tables.
    user="gov06_auditor"
    host="%"
    password="ci-isolated-auditor-only"
    q(f"CREATE USER '{user}'@'{host}' IDENTIFIED BY %s",(password,))
    for table in ("treasury_accounts","treasury_ledger","government_bond_issues"):
        q(f"GRANT SELECT ON \`{dbname}\`.\`{table}\` TO '{user}'@'{host}'")
    q(f"GRANT EXECUTE ON PROCEDURE \`{dbname}\`.gov_finance_assert_integrity "
      f"TO '{user}'@'{host}'")
    restricted={**DB,"user":user,"password":password}
    assert call_audit(restricted)[0]=="PASS"
    deny("UPDATE treasury_accounts SET balance=balance+1 WHERE code='state'",
         credentials=restricted)
    deny("INSERT INTO treasury_ledger(event_key,amount_delta,balance_after,source) "
         "VALUES('evil:credit',100,100,'evil')",credentials=restricted)
    deny("CALL gov_bond_role_set('bond:evil:role',1,2,'DIRECTOR',1)",
         credentials=restricted)
    deny("CALL gov_bond_buy('bond:evil:buy','GOV-000001',1,1,0)",
         credentials=restricted)
    deny("CALL treasury_post('evil:cash',999999,'tax',NULL,'fake')",
         credentials=restricted)

    # The read-only audit must actually detect an independent cash inflation,
    # and the test must not leave it in the database.
    with mysql.connector.connect(**DB) as db,db.cursor() as c:
        db.autocommit=False
        c.execute("UPDATE treasury_accounts SET balance=balance+1 WHERE code='state'")
        try:
            c.execute("CALL gov_finance_assert_integrity()")
            raise AssertionError("Audit failed to detect tampered treasury cash")
        except mysql.connector.Error as error:
            assert error.sqlstate=="45000",error
        finally:
            db.rollback()
    assert call_audit()==baseline

    # 64 concurrent accounting reads must return the same sums and never hang.
    start=time.monotonic()
    with ThreadPoolExecutor(max_workers=8) as pool:
        rows=list(pool.map(lambda _:call_audit(),range(64)))
    assert all(row==baseline for row in rows)
    elapsed=time.monotonic()-start
    assert elapsed<25,elapsed
    assert call_audit()==baseline

    print("GOV-06 MariaDB UAT: PASS (22 append-only triggers, cash inflation "
          "detected, no forged restricted-admin calls, 64 concurrent audits, "
          "stable reconciled balance; audit elapsed %.2f s)"%elapsed)

if __name__=="__main__":
    main()
