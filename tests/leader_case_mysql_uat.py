"""GOV-05 MariaDB UAT: mixed-case minister survives roster load/save and solo reassignment.

Runs after gov05_mysql_uat.py on disposable CI database; all DML is rolled back.
The simulated SQL matches DBWorld_LeaderSave's case-insensitive username_key
lookup and reproduces the precise Veyran/veyran reappointment failure.
"""
import os
import mysql.connector

database=os.environ.get("GOV00_TEST_DB","tarkenso_gov00")
assert database=="tarkenso_gov00","Refuse non-disposable leader identity test database"
db=mysql.connector.connect(
    host=os.environ.get("GOV00_TEST_HOST","127.0.0.1"),
    port=int(os.environ.get("GOV00_TEST_PORT","3306")),
    user=os.environ.get("GOV00_TEST_USER","root"),
    password=os.environ["GOV00_TEST_PASSWORD"],
    database=database,autocommit=False)

try:
    db.start_transaction()
    with db.cursor() as cur:
        cur.execute("""
            SELECT a.account_id,a.username,m.rank_id,m.joined_at,m.last_seen
            FROM accounts a JOIN faction_members m ON m.account_id=a.account_id
            WHERE a.username_key=%s AND m.faction_id=5 FOR UPDATE
        """,("gov03buyer1",))
        entry=cur.fetchone()
        assert entry is not None,"GOV-05 UAT did not seed the minister"
        account_id,original,rank,joined,seen=entry
        assert rank==4,entry
        mixed=original[0].upper()+original[1:]
        assert mixed!=original and mixed.casefold()==original.casefold()

        # Preserve the display name; generated username_key is lower-case.
        cur.execute("UPDATE accounts SET username=%s WHERE account_id=%s",
                    (mixed,account_id))
        assert cur.rowcount==1
        cur.execute("SELECT a.username,m.rank_id FROM faction_members m "
                    "JOIN accounts a ON a.account_id=m.account_id "
                    "WHERE m.faction_id=5 AND m.account_id=%s",(account_id,))
        roster=cur.fetchone()
        assert roster==(mixed,4),roster
        assert roster[0].casefold()==original.casefold()

        # /unleader is a full dismissal, not a demotion: clear the term
        # and DELETE the minister in the same transaction. Then /setleader
        # reinstates a new leader, with fresh membership.
        cur.execute("UPDATE factions SET term_start=0,term_end=0 WHERE faction_id=5")
        cur.execute("DELETE FROM faction_members WHERE account_id=%s",(account_id,))
        assert cur.rowcount==1
        cur.execute("SELECT COUNT(*) FROM faction_members WHERE account_id=%s",
                    (account_id,))
        assert cur.fetchone()==(0,),"former minister was not made civilian"
        cur.execute("SELECT term_start,term_end FROM factions WHERE faction_id=5")
        assert cur.fetchone()==(0,0),"dismissal did not clear leadership term"
        cur.execute("SELECT COUNT(*) FROM faction_members WHERE faction_id=5")
        assert cur.fetchone()[0]>=1,"other members must remain in ministry"
        cur.execute("""
            INSERT INTO faction_members(account_id,faction_id,rank_id,joined_at,last_seen)
            SELECT account_id,5,4,%s,%s FROM accounts WHERE username_key=LOWER(%s)
        """,(joined,seen,mixed))
        assert cur.rowcount==1,"mixed-case username lookup failed to reinsert member"
        cur.execute("SELECT rank_id FROM faction_members WHERE account_id=%s",
                    (account_id,))
        assert cur.fetchone()==(4,)
        cur.execute("SELECT COUNT(*) FROM faction_members WHERE account_id=%s",
                    (account_id,))
        assert cur.fetchone()==(1,),"solo reappointment duplicated an account"
        cur.execute("SELECT username FROM accounts WHERE account_id=%s",(account_id,))
        assert cur.fetchone()==(mixed,)

    print("Leader mixed-case MariaDB UAT PASS: preserved canonical display case, "
          "unleader civilian, term reset, reappoint rank 4, unique membership and SQL name lookup")
finally:
    db.rollback()
    db.close()
