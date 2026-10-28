"""GOV-07.1 durable PayDay coupon notices, run AFTER GOV-07 UAT.

Uses the disposable CI MariaDB schema. A transactionally rolled-back
synthetic multi-issue set tests top-3 aggregation; it never survives UAT.
"""
from datetime import datetime,timezone
import os
import mysql.connector

database=os.environ.get("GOV00_TEST_DB","tarkenso_gov00")
assert database=="tarkenso_gov00","Refuse non-disposable GOV-07 notice UAT database"
db=mysql.connector.connect(
    host=os.environ.get("GOV00_TEST_HOST","127.0.0.1"),
    port=int(os.environ.get("GOV00_TEST_PORT","3306")),
    user=os.environ.get("GOV00_TEST_USER","root"),
    password=os.environ["GOV00_TEST_PASSWORD"],
    database=database,autocommit=True)

def rows(sql,params=()):
    with db.cursor() as cur:
        cur.execute(sql,params)
        return cur.fetchall()

def run(sql,params=()):
    with db.cursor() as cur:
        cur.execute(sql,params)
        return cur.rowcount

# Same SQL aggregate used by the async Pawn callback.
AGGREGATE="""
SELECT v.issue_code,CAST(v.amount AS CHAR) AS amount,
       CAST(SUM(v.amount) OVER() AS CHAR) AS total,
       COUNT(*) OVER() AS series_count,
       CAST(MAX(v.last_id) OVER() AS CHAR) AS max_id
FROM (
    SELECT i.issue_code,SUM(q.amount) AS amount,MAX(q.notice_id) AS last_id
    FROM government_bond_coupon_notice_queue q
    JOIN government_bond_issues i ON i.issue_id=q.issue_id
    LEFT JOIN government_bond_coupon_notice_deliveries d ON d.notice_id=q.notice_id
    WHERE q.account_id=%s AND d.notice_id IS NULL
    GROUP BY q.issue_id,i.issue_code
) v
ORDER BY v.amount DESC,v.issue_code LIMIT 3
"""
ACK="""
INSERT IGNORE INTO government_bond_coupon_notice_deliveries(notice_id,account_id)
SELECT q.notice_id,q.account_id FROM government_bond_coupon_notice_queue q
LEFT JOIN government_bond_coupon_notice_deliveries d ON d.notice_id=q.notice_id
WHERE q.account_id=%s AND q.notice_id<=%s AND d.notice_id IS NULL
"""

def main():
    assert rows("SELECT version FROM schema_migrations WHERE version=27")==[(27,)]
    receipt=rows("SELECT issue_id,account_id FROM government_bond_coupon_receipts "
                 "WHERE event_key='bond:gov07:coupon:offline' LIMIT 1")
    assert receipt,"GOV-07 UAT must run before GOV-07.1"
    issue,account=receipt[0]
    notice=rows("SELECT notice_id,amount,hour_key FROM government_bond_coupon_notice_queue "
                "WHERE issue_id=%s AND account_id=%s",(issue,account))
    assert notice and sum(x[1] for x in notice)>0,notice
    assert rows("SELECT COUNT(*) FROM government_bond_coupon_notice_queue "
                "WHERE amount=0")==[(0,)]
    wallet=rows("SELECT accrued FROM government_bond_coupon_wallets "
                "WHERE issue_id=%s AND account_id=%s",(issue,account))
    assert sum(x[1] for x in notice)==wallet[0][0],(notice,wallet)
    # The coupon was paid in GOV-07 UAT, but the notification remains queued
    # until sent; obtaining cash never counts as a chat delivery.
    pending=rows(AGGREGATE,(account,))
    assert pending,pending
    max_id=int(pending[0][4])
    pending_total=int(pending[0][2])
    assert pending_total>0
    before=rows("SELECT COUNT(*) FROM government_bond_coupon_notice_deliveries "
                "WHERE account_id=%s",(account,))[0][0]
    marked=run(ACK,(account,max_id))
    assert marked>0
    assert run(ACK,(account,max_id))==0,"ACK replay inserted duplicates"
    assert rows(AGGREGATE,(account,))==[],"Acked events reappear"
    assert rows("SELECT COUNT(*) FROM government_bond_coupon_notice_deliveries "
                "WHERE account_id=%s",(account,))[0][0]==before+marked
    # The high-water mark is a snapshot: a new event must not be swallowed
    # if another PayDay commits before ACK of the previous notification.
    other=rows("SELECT issue_id FROM government_bond_issues "
               "WHERE issue_id<>%s ORDER BY issue_id LIMIT 4",(issue,))
    assert len(other)==4,other
    hour=int(datetime.now(timezone.utc).timestamp())//3600 + 20
    db.start_transaction()
    try:
        for offset,(other_issue,) in enumerate(other):
            run("INSERT INTO government_bond_coupon_notice_queue "
                "(issue_id,account_id,hour_key,amount) VALUES(%s,%s,%s,%s)",
                (other_issue,account,hour+offset,offset+2))
        grouped=rows(AGGREGATE,(account,))
        assert len(grouped)==3,grouped
        assert grouped[0][3]==4,grouped
        assert grouped[0][2]=="14",grouped
        assert [int(x[1]) for x in grouped]==[5,4,3],grouped
        assert run(ACK,(account,max_id))==0
        assert rows(AGGREGATE,(account,))==grouped
    finally:
        db.rollback()
    assert rows(AGGREGATE,(account,))==[]
    print("GOV-07.1 notice UAT PASS: durable positive events, independent "
          "coupon collection, offline backlog, top-3 total, ACK replay and "
          "high-water snapshot")

if __name__=="__main__":
    try:main()
    finally:db.close()
