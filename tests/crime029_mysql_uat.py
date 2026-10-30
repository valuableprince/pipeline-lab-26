"""CRIME-02.9 MariaDB UAT: activation, solo maintenance, idempotency, caps."""
from crime02_mysql_uat import CFG, run, scalar
from crime028_mysql_uat import service
import mysql.connector


def main():
    db=mysql.connector.connect(**CFG)
    try:
        a=scalar(db,"SELECT account_id FROM accounts WHERE username='CrimeSoloA'")
        b=scalar(db,"SELECT account_id FROM accounts WHERE username='CrimeSoloB'")
        assert a and b
        assert scalar(db,"SELECT COUNT(*) FROM schema_migrations WHERE version=40")==1
        assert scalar(db,"SELECT COUNT(*) FROM crime_syndicate_asset_services WHERE faction_id=6 AND contact_id=2")==2

        # Both new assets were activated in CRIME-02.8 UAT, not on purchase.
        assert scalar(db,"SELECT COUNT(*) FROM crime_syndicate_assets WHERE faction_id=6 AND contact_id=2 AND activated_at>0")==2
        assert not service(db,a,6,2,1)  # Cannot double collect on the Y key.
        assert not service(db,b,6,2,1)  # A player from faction 7 cannot collect.

        before_wallet=scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))
        before_standing=scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=6 AND contact_id=2")
        other=scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=7")
        run(db,"UPDATE crime_syndicate_assets SET last_serviced_at=UNIX_TIMESTAMP()-1900 WHERE faction_id=6 AND contact_id=2 AND asset_code=1")
        before=scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=6")
        assert service(db,a,6,2,1)
        assert scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=6")==before+2
        assert not service(db,a,6,2,1)
        assert scalar(db,"SELECT service_count FROM crime_syndicate_assets WHERE faction_id=6 AND contact_id=2 AND asset_code=1")==2
        assert scalar(db,"SELECT COUNT(*) FROM crime_syndicate_asset_services WHERE faction_id=6 AND contact_id=2 AND asset_code=1")==2
        assert scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=7")==other

        # Informant service improves relations, not organizational wallet.
        run(db,"UPDATE crime_syndicate_assets SET last_serviced_at=UNIX_TIMESTAMP()-1900 WHERE faction_id=6 AND contact_id=2 AND asset_code=2")
        assert service(db,a,6,2,2)
        assert scalar(db,"SELECT standing FROM crime_syndicate_contacts WHERE faction_id=6 AND contact_id=2")==before_standing+2
        assert not service(db,a,6,2,2)
        assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))==before_wallet

        # The ledger records the actual capped +1 / +0, not fictional +2.
        run(db,"UPDATE crime_syndicate_progress SET influence=999999 WHERE faction_id=6")
        run(db,"UPDATE crime_syndicate_assets SET last_serviced_at=UNIX_TIMESTAMP()-1900 WHERE faction_id=6 AND contact_id=2 AND asset_code=1")
        assert service(db,a,6,2,1)
        assert scalar(db,"SELECT influence FROM crime_syndicate_progress WHERE faction_id=6")==1000000
        assert scalar(db,"SELECT influence_delta FROM crime_syndicate_asset_services WHERE faction_id=6 AND contact_id=2 AND asset_code=1 ORDER BY service_count DESC LIMIT 1")==1
        run(db,"UPDATE crime_syndicate_assets SET last_serviced_at=UNIX_TIMESTAMP()-1900 WHERE faction_id=6 AND contact_id=2 AND asset_code=1")
        assert service(db,a,6,2,1)
        assert scalar(db,"SELECT influence_delta FROM crime_syndicate_asset_services WHERE faction_id=6 AND contact_id=2 AND asset_code=1 ORDER BY service_count DESC LIMIT 1")==0
        assert scalar(db,"SELECT balance FROM accounts WHERE account_id=%s",(a,))==before_wallet

        # The local service history cannot ever be applied to faction 8.
        assert scalar(db,"SELECT COUNT(*) FROM crime_syndicate_asset_services WHERE faction_id=8")==0
        print("CRIME-02.9 MariaDB UAT: PASS (world mission cooldown, idempotence, caps, faction ownership and no wallet impact)")
    finally:
        db.close()


if __name__=="__main__":
    main()
