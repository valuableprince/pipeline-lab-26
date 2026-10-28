"""ADMIN-MONEY-02: MariaDB UAT for the atomic, audited administrator treasury mint.

Run on disposable GOV-00..06 schema after all existing tests. No production writes.
"""
from pathlib import Path
import os
import mysql.connector

CONFIG = dict(
    host=os.environ.get("GOV00_TEST_HOST","127.0.0.1"),
    port=int(os.environ.get("GOV00_TEST_PORT","3306")),
    user=os.environ.get("GOV00_TEST_USER","root"),
    password=os.environ["GOV00_TEST_PASSWORD"],
    database=os.environ.get("GOV00_TEST_DB","tarkenso_gov00"),
    autocommit=True,
)

def read(db, sql, args=()):
    with db.cursor() as cur:
        cur.execute(sql,args)
        return cur.fetchall()

def execute_post(db,event,amount,actor,note="admin_cash_mint"):
    with db.cursor() as cur:
        cur.execute("CALL treasury_post(%s,%s,%s,%s,%s)",
                    (event,amount,"admin_topup",actor,note))
        data=cur.fetchone()
        while cur.nextset():
            pass
        return data

def rejected(db,event,amount,actor,note):
    try:
        execute_post(db,event,amount,actor,note)
    except mysql.connector.Error:
        return
    raise AssertionError("Unexpected SQL success")

def main():
    with mysql.connector.connect(**CONFIG) as db:
        admin_id=read(db,
            "SELECT account_id FROM accounts WHERE username='gov03buyer1'")[0][0]
        old=read(db,
            "SELECT balance,bond_fund_reserved,bond_fund_committed,bond_payout_reserved "
            "FROM treasury_accounts WHERE code='state'")[0]
        old_user_balance=read(db,
            "SELECT balance FROM accounts WHERE account_id=%s",(admin_id,))[0][0]
        event="admtr:uat:admin5:1"
        receipt=execute_post(db,event,500000,admin_id)
        assert receipt[0]==old[0]+500000 and receipt[-1]==0,receipt
        # Idempotent replay does not mint money again.
        replay=execute_post(db,event,500000,admin_id)
        assert replay[0]==receipt[0] and replay[-1]==1,replay
        rejected(db,event,500001,admin_id,"admin_cash_mint")
        rejected(db,event,500000,admin_id,"altered_note")
        rejected(db,"admtr:uat:bad:zero",0,admin_id,"admin_cash_mint")
        rejected(db,"admtr:uat:bad:too-large",1000000001,admin_id,"admin_cash_mint")
        assert read(db,"SELECT COUNT(*),SUM(amount_delta),MAX(actor_account_id),"
                   "MAX(source),MAX(note) FROM treasury_ledger WHERE event_key=%s",
                   (event,))==[(1,500000,admin_id,"admin_topup","admin_cash_mint")]
        now=read(db,
            "SELECT balance,bond_fund_reserved,bond_fund_committed,bond_payout_reserved "
            "FROM treasury_accounts WHERE code='state'")[0]
        assert now[0]==old[0]+500000 and now[1:]==old[1:],(old,now)
        assert read(db,
            "SELECT balance FROM accounts WHERE account_id=%s",
            (admin_id,))[0][0]==old_user_balance
        with db.cursor() as cur:
            cur.execute("CALL gov_finance_assert_integrity()")
            integrity=cur.fetchone()
            while cur.nextset():
                pass
        assert integrity is not None and integrity[0]=="PASS",integrity
        print("ADMIN-MONEY-02 MariaDB UAT PASS: ledger atomicity, idempotent replay, "
              "validated amount, actor identity, unchanged wallet/reserves")

if __name__=="__main__":
    main()
