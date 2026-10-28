"""GOV-07 disposable MariaDB UAT: offline coupons and wallet settlement.

Test-only historical order timestamp is injected by temporarily replacing the
GOV-06 immutable-order guard on a disposable CI database, then restoring it.
Never run this fixture against any production or persistent database.
"""
import os
from datetime import datetime,timezone,timedelta
import mysql.connector

dbname=os.environ.get("GOV00_TEST_DB","tarkenso_gov00")
assert dbname=="tarkenso_gov00","GOV-07 coupon UAT requires the disposable test DB"
db=mysql.connector.connect(
    host=os.environ.get("GOV00_TEST_HOST","127.0.0.1"),
    port=int(os.environ.get("GOV00_TEST_PORT","3306")),
    user=os.environ.get("GOV00_TEST_USER","root"),
    password=os.environ["GOV00_TEST_PASSWORD"],
    database=dbname,autocommit=True
)

def read(sql,parameters=()):
    with db.cursor() as cursor:
        cursor.execute(sql,parameters)
        return cursor.fetchall()

def execute(sql,parameters=()):
    with db.cursor() as cursor:
        cursor.execute(sql,parameters)

def call(name,*parameters):
    with db.cursor() as cursor:
        cursor.execute("CALL "+name+"("+",".join(["%s"]*len(parameters))+")",parameters)
        result=cursor.fetchone()
        while cursor.nextset(): pass
        return result

def deny(name,*parameters):
    try:call(name,*parameters)
    except mysql.connector.Error:return
    raise AssertionError("Unexpected SQL success for "+name)

def main():
    assert read("SELECT version FROM schema_migrations WHERE version=26")==[(26,)]
    item=read("""
      SELECT o.order_id,o.issue_id,o.account_id,i.maturity_at,
             o.quantity,i.face_value,i.yield_basis_points
      FROM government_bond_orders o
      JOIN government_bond_issues i ON i.issue_id=o.issue_id
      WHERE i.status='OFFERING' AND i.maturity_at>UTC_TIMESTAMP()+INTERVAL 5 DAY
      ORDER BY (o.quantity*(i.face_value*i.yield_basis_points DIV 10000)) DESC,
               o.order_id DESC LIMIT 1
    """)
    assert item,"No active purchase available for coupon test"
    order_id,issue_id,account_id,maturity,qty,face,rate=item[0]
    total=qty*(face*rate//10000)
    assert total>0,(item,total)
    # Simulate an offline purchase held for multiple PayDays. Restore the guard.
    assert read("SELECT COUNT(*) FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='g06_bond_orders_upd'")==[(1,)]
    execute("DROP TRIGGER g06_bond_orders_upd")
    try:
        execute("UPDATE government_bond_orders SET created_at=UTC_TIMESTAMP()-INTERVAL 25 DAY WHERE order_id=%s",(order_id,))
    finally:
        execute("""CREATE TRIGGER g06_bond_orders_upd BEFORE UPDATE ON government_bond_orders
                 FOR EACH ROW BEGIN SIGNAL SQLSTATE '45000'
                 SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events'; END""")
    current_hour=int(datetime.now(timezone.utc).timestamp())//3600
    # Scheduler does not use online player state. One post settles every order
    # of the issue, with the exact one-hour event key.
    posted=call("gov_bond_coupon_accrue",issue_id,current_hour)
    assert posted[0]==issue_id and posted[1]>=0 and posted[-1]==0,posted
    assert call("gov_bond_coupon_accrue",issue_id,current_hour)[-1]==1
    wallet=read("SELECT accrued,collected,converted FROM government_bond_coupon_wallets "
                "WHERE issue_id=%s AND account_id=%s",(issue_id,account_id))
    assert wallet and wallet[0][0]>0,wallet
    accrued,paid,converted=wallet[0]
    assert paid==converted==0 and accrued<=total,(accrued,total)
    assert read("SELECT COUNT(*) FROM government_bond_coupon_events "
                "WHERE issue_id=%s AND hour_key=%s",(issue_id,current_hour))==[(1,)]

    before_money=read("SELECT balance,row_version,txn_serial FROM accounts WHERE account_id=%s",(account_id,))[0]
    before_treasury=read("SELECT balance FROM treasury_accounts WHERE code='state'")[0][0]
    amount=min(accrued,1)
    event="bond:gov07:coupon:offline"
    receipt=call("gov_bond_coupon_collect",event,issue_id,account_id,amount,before_money[1])
    assert receipt[-1]==0 and receipt[1]==before_money[0]+amount,receipt
    assert call("gov_bond_coupon_collect",event,issue_id,account_id,amount,before_money[1])[-1]==1
    deny("gov_bond_coupon_collect",event,issue_id,account_id,amount+1,before_money[1])
    after=read("SELECT balance,row_version,txn_serial FROM accounts WHERE account_id=%s",(account_id,))[0]
    assert after==(before_money[0]+amount,before_money[1]+1,before_money[2]+1)
    assert read("SELECT balance FROM treasury_accounts WHERE code='state'")[0][0]==before_treasury-amount
    assert read("SELECT COUNT(*),SUM(amount_delta) FROM treasury_ledger WHERE event_key=%s",(event,))==[(1,-amount)]
    assert read("SELECT COUNT(*) FROM government_bond_coupon_receipts WHERE event_key=%s",(event,))==[(1,)]
    assert read("SELECT accrued-collected-converted FROM government_bond_coupon_wallets WHERE issue_id=%s AND account_id=%s",
                (issue_id,account_id))==[(accrued-amount,)]
    assert call("gov_finance_assert_integrity")[0]=="PASS"
    print("GOV-07 UAT PASS: PayDay offline accrual, idempotence, conditional wallet, "
          "manual collection, guarded reserve, no double transfer, integrity")

if __name__=="__main__":
    try:main()
    finally:db.close()
