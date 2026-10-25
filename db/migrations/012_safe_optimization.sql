-- TARKENSO / DB-OPT-012, MariaDB 10.11, additive changes on top of v11.
-- STOP the game server and back up the database before applying.
-- No user rows are modified. Keep a matching server AMX: DBPrimary_Init requires v12.
-- MariaDB DDL implicitly commits: on a partial failure run the companion rollback
-- while the server is stopped, then inspect the cause before retrying.
DELIMITER $$
DROP PROCEDURE IF EXISTS tarkenso_apply_012$$
CREATE PROCEDURE tarkenso_apply_012()
BEGIN
    IF (SELECT COUNT(*) FROM schema_migrations WHERE version=11) <> 1
       OR (SELECT COUNT(*) FROM schema_migrations WHERE version>=12) <> 0 THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT='DB-OPT-012 requires schema v11 and no later migrations';
    END IF;

    IF (SELECT COUNT(*) FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='bans'
          AND COLUMN_NAME='username') <> 1
       OR (SELECT COUNT(*) FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA=DATABASE()
          AND TABLE_NAME='municipal_truck_deliveries'
          AND COLUMN_NAME IN ('account_id','completed_at','delivery_id')) <> 3 THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT='DB-OPT-012 required v11 columns are missing';
    END IF;

    -- Existing mixed-case bans must not be collapsed or silently overwritten.
    IF EXISTS (SELECT 1 FROM bans GROUP BY LOWER(username) HAVING COUNT(*)>1) THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT='DB-OPT-012 case-insensitive duplicate bans: resolve manually';
    END IF;

    IF EXISTS (SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='bans'
          AND COLUMN_NAME='username_key')
       OR EXISTS (SELECT 1 FROM information_schema.STATISTICS
        WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='bans'
          AND INDEX_NAME='uq_bans_username_key')
       OR EXISTS (SELECT 1 FROM information_schema.STATISTICS
        WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='municipal_truck_deliveries'
          AND INDEX_NAME='idx_mtd_account_completed') THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT='DB-OPT-012 target column/index already exists: inspect partial migration';
    END IF;

    -- Preserve the original bans.username PK, and support unregistered nicknames.
    -- Mirrors the established accounts.username_key convention.
    ALTER TABLE bans
        ADD COLUMN username_key VARCHAR(20) CHARACTER SET ascii
            COLLATE ascii_bin GENERATED ALWAYS AS (LOWER(username)) STORED,
        ADD UNIQUE KEY uq_bans_username_key (username_key);

    -- Forward-compatible efficient per-player delivery history queries.
    -- MariaDB may replace the implicit FK index with this composite index;
    -- the companion rollback explicitly restores fk_mtd_account first.
    ALTER TABLE municipal_truck_deliveries
        ADD INDEX idx_mtd_account_completed (account_id,completed_at,delivery_id);

    INSERT INTO schema_migrations(version) VALUES (12);
END$$
DELIMITER ;
CALL tarkenso_apply_012();
DROP PROCEDURE tarkenso_apply_012;
