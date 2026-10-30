"""CRIME-02.8 MariaDB: rank gates, two assets, exact-once faction progress."""
from crime02_mysql_uat import CFG, run, scalar, begin
from crime027_mysql_uat import courier
import mysql.connector

def buy(db, acc, faction, contact, kind):
    cost,standing=(15,10) if kind==1 else (30,20)
    begin(db)
    try:
        n,_=run(db,"""UPDATE crime_syndicate_progress p SET p.influence=p.influence-%s
          WHERE p.faction_id=%s AND p.influence>=%s
          AND EXISTS(SELECT 1 FROM faction_members m WHERE m.account_id=%s
            AND m.faction_id=p.faction_id AND m.rank_id>=4)
          AND EXISTS(SELECT 1 FROM crime_syndicate_contacts c
            WHERE c.faction_id=p.faction_id AND c.contact_id=%s AND c.standing>=%s)
          AND NOT EXISTS(SELECT 1 FROM crime_syndicate_assets a
            WHERE a.faction_id=p.faction_id AND a.contact_id=%s AND a.asset_code=%s)""",
            (cost,faction,cost,acc,contact,standing,contact,kind))
        if n!=1:
            run(db,"ROLLBACK")
            return False
        n,_=run(db,"""INSERT INTO crime_syndicate_assets
          (faction_id,contact_id,asset_code,cost_influence,acquired_by,acquired_at)
          VALUES(%s,%s,%s,%s,%s,UNIX_TIMESTAMP())""",
          (faction,contact,kind,cost,acc))
        assert n==1
        run(db,"COMMIT")
        return True
    except Exception:
        run(db,"ROLLBACK")
        raise

def main():
    db=mysql.connector.connect(**CFG)
    try:
        a=scalar(db,"SELECT account_id FROM accounts WHERE username='CrimeSoloA'")
        b=scalar(db,"SELECT account_id FROM accounts WHERE username='CrimeSoloB'")
        assert a and b
        assert scalar(db,"SELECT COUNT(*) FROM schema_migrations WHERE version=39")==1
        assert scalar(db,"SELECT COUNT(*) FROM crime_syndicate_progress")==3
        assert scalar(db,"SELECT COUNT(*) FROM crime_syndicate_contacts")==9
        other=scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=7")
        run(db,"UPDATE crime_syndicate_progress SET influence=10 WHERE faction_id=6")
        run(db,"UPDATE crime_syndicate_contacts SET standing=9 WHERE faction_id=6 AND contact_id=2")
        assert not buy(db,a,6,2,1)  # Rank 1 cannot buy
        run(db,"UPDATE faction_members SET rank_id=4 WHERE account_id=%s AND faction_id=6",(a,))
        assert not buy(db,a,6,2,1)  # Low standing and funds
        run(db,"UPDATE crime_syndicate_progress SET influence=70 WHERE faction_id=6")
        assert not buy(db,a,6,2,1)  # Standing gate
        run(db,"UPDATE crime_syndicate_contacts SET standing=25 WHERE faction_id=6 AND contact_id=2")
        assert buy(db,a,6,2,1)
        assert buy(db,a,6,2,2)
        assert scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=6")==25
        assert not buy(db,a,6,2,1)
        assert not buy(db,a,6,2,2)
        assert scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=6")==25
        assert scalar(db,"SELECT COUNT(*) FROM crime_syndicate_assets WHERE faction_id=6 AND contact_id=2")==2
        before=scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=6 AND contact_id=2")
        cid=courier(db,a)
        assert scalar(db,"SELECT influence_delta FROM crime_syndicate_influence_events WHERE contract_id=%s",(cid,))==3
        assert scalar(db,"SELECT standing_delta FROM crime_syndicate_influence_events WHERE contract_id=%s",(cid,))==3
        assert scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=6")==28
        assert scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=6 AND contact_id=2")==before+3
        assert scalar(db,"SELECT COUNT(*) FROM crime_syndicate_influence_events WHERE contract_id=%s",(cid,))==1
        assert scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=7")==other
        assert scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=6 AND contact_id=3")==0
        assert not buy(db,b,6,2,1)
        run(db,"UPDATE faction_members SET rank_id=1 WHERE account_id=%s AND faction_id=6",(a,))
        print("CRIME-02.8 MariaDB UAT: PASS (organization ownership, rank, assets, bonuses, replay, solo)")
    finally:
        db.close()

if __name__=="__main__":
    main()
