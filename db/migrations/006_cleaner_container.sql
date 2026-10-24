-- TARKENSO / JOB-CLEANER-04: persistent Fort Carson refuse container
-- Apply once AFTER 005_npc_core.sql (schema v5).
-- MariaDB 10.11; existing accounts, cleaner profiles and routes are not modified.
CREATE TABLE cleaner_container (
  container_id TINYINT UNSIGNED NOT NULL PRIMARY KEY,
  bin_units TINYINT UNSIGNED NOT NULL DEFAULT 0,
  overflow_units TINYINT UNSIGNED NOT NULL DEFAULT 0,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  CONSTRAINT chk_cleaner_container_id CHECK (container_id = 1),
  CONSTRAINT chk_cleaner_bin CHECK (bin_units BETWEEN 0 AND 60),
  CONSTRAINT chk_cleaner_overflow CHECK (overflow_units BETWEEN 0 AND 40),
  CONSTRAINT chk_cleaner_overflow_requires_full CHECK (overflow_units = 0 OR bin_units = 60)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO cleaner_container (container_id, bin_units, overflow_units)
VALUES (1,0,0);

INSERT INTO schema_migrations (version) VALUES (6);
