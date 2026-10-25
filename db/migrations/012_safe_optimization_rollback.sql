-- TARKENSO / DB-OPT-012 rollback. STOP the game server, restore the pre-v12
-- AMX first, take a fresh backup, then execute this SQL.
-- Also repairs a partial v12 DDL application before its migration marker exists.
-- Drops ONLY objects introduced by v12; no account/ban/delivery rows are removed.
DELIMITER $$
DROP PROCEDURE IF EXISTS tarkenso_rollback_012$$
CREATE PROCEDURE tarkenso_rollback_012()
BEGIN
    IF EXISTS (SELECT 1 FROM schema_migrations WHERE version>12) THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT='Cannot roll back v12 with newer migrations present';
    END IF;

    IF EXISTS (SELECT 1 FROM information_schema.STATISTICS
        WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='municipal_truck_deliveries'
          AND INDEX_NAME='idx_mtd_account_completed') THEN
        ALTER TABLE municipal_truck_deliveries
            DROP INDEX idx_mtd_account_completed;
    END IF;

    IF EXISTS (SELECT 1 FROM information_schema.STATISTICS
        WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='bans'
          AND INDEX_NAME='uq_bans_username_key') THEN
        ALTER TABLE bans DROP INDEX uq_bans_username_key;
    END IF;

    IF EXISTS (SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='bans'
          AND COLUMN_NAME='username_key') THEN
        ALTER TABLE bans DROP COLUMN username_key;
    END IF;

    DELETE FROM schema_migrations WHERE version=12;
END$$
DELIMITER ;
CALL tarkenso_rollback_012();
DROP PROCEDURE tarkenso_rollback_012;
