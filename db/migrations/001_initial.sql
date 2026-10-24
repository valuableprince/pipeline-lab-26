-- TARKENSO / DB-MIGRATION-01
-- Run in an EMPTY database using a dedicated migration account.
-- MySQL 8.x or MariaDB 10.6+; every data table is InnoDB.
-- DDL implicitly commits in MySQL: create schema BEFORE running data import.
CREATE TABLE IF NOT EXISTS schema_migrations (
  version INT UNSIGNED NOT NULL PRIMARY KEY,
  applied_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
INSERT INTO schema_migrations (version) VALUES (1);

CREATE TABLE accounts (
  account_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  username VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  password_hash CHAR(60) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  background TINYINT UNSIGNED NOT NULL,
  mute_until INT UNSIGNED NOT NULL DEFAULT 0,
  balance BIGINT UNSIGNED NOT NULL DEFAULT 0,
  health DECIMAL(5,2) NOT NULL DEFAULT 100.00,
  armour DECIMAL(5,2) NOT NULL DEFAULT 0.00,
  cleaner_paid INT NOT NULL DEFAULT -1,
  txn_serial BIGINT UNSIGNED NOT NULL DEFAULT 0,
  race_starts BIGINT UNSIGNED NOT NULL DEFAULT 0,
  race_finishes BIGINT UNSIGNED NOT NULL DEFAULT 0,
  race_wins BIGINT UNSIGNED NOT NULL DEFAULT 0,
  race_best INT UNSIGNED NOT NULL DEFAULT 0,
  race_earned BIGINT UNSIGNED NOT NULL DEFAULT 0,
  race_last_paid INT UNSIGNED NOT NULL DEFAULT 0,
  source_version TINYINT UNSIGNED NOT NULL DEFAULT 4,
  row_version BIGINT UNSIGNED NOT NULL DEFAULT 0,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  CONSTRAINT uq_accounts_username UNIQUE(username),
  CONSTRAINT chk_accounts_background CHECK (background BETWEEN 1 AND 3),
  CONSTRAINT chk_accounts_balance CHECK (balance <= 1000000000),
  CONSTRAINT chk_accounts_vitals CHECK (health BETWEEN 1 AND 100 AND armour BETWEEN 0 AND 100),
  CONSTRAINT chk_accounts_cleaner CHECK (cleaner_paid BETWEEN -1 AND 1000000),
  CONSTRAINT chk_accounts_race CHECK (
    race_finishes <= race_starts AND race_wins <= race_finishes
    AND (race_best = 0 OR race_best BETWEEN 1 AND 300)
  )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE cleaner_profiles (
  account_id BIGINT UNSIGNED NOT NULL PRIMARY KEY,
  xp INT UNSIGNED NOT NULL DEFAULT 0,
  collected INT UNSIGNED NOT NULL DEFAULT 0,
  earned INT UNSIGNED NOT NULL DEFAULT 0,
  night_completed INT UNSIGNED NOT NULL DEFAULT 0,
  night_last_paid INT UNSIGNED NOT NULL DEFAULT 0,
  CONSTRAINT fk_cleaner_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_cleaner_values CHECK (xp = collected AND
    earned = collected * 25 + night_completed * 100)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE admin_roles (
  account_id BIGINT UNSIGNED NOT NULL PRIMARY KEY,
  admin_level TINYINT UNSIGNED NOT NULL,
  event_role BOOLEAN NOT NULL DEFAULT FALSE,
  CONSTRAINT fk_admin_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_admin_level CHECK (admin_level <= 6)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- No FK: bans may target names that have never registered.
CREATE TABLE bans (
  username VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
  banned_until INT UNSIGNED NOT NULL,
  reason VARCHAR(96) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE factions (
  faction_id TINYINT UNSIGNED NOT NULL PRIMARY KEY,
  is_enabled BOOLEAN NOT NULL DEFAULT FALSE,
  term_start INT UNSIGNED NOT NULL DEFAULT 0,
  term_end INT UNSIGNED NOT NULL DEFAULT 0,
  CONSTRAINT chk_faction_term CHECK (
    (term_start = 0 AND term_end = 0) OR (term_start > 0 AND term_end > term_start)
  )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- PK(account_id) prevents membership in two factions.
CREATE TABLE faction_members (
  account_id BIGINT UNSIGNED NOT NULL PRIMARY KEY,
  faction_id TINYINT UNSIGNED NOT NULL,
  rank_id TINYINT UNSIGNED NOT NULL,
  joined_at INT UNSIGNED NOT NULL,
  last_seen INT UNSIGNED NOT NULL,
  CONSTRAINT fk_member_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT fk_member_faction FOREIGN KEY (faction_id)
    REFERENCES factions(faction_id) ON DELETE RESTRICT,
  CONSTRAINT chk_member_rank CHECK (rank_id BETWEEN 1 AND 10),
  INDEX idx_member_faction (faction_id, rank_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE cleaner_districts (
  district_id TINYINT UNSIGNED NOT NULL PRIMARY KEY,
  pollution TINYINT UNSIGNED NOT NULL,
  CONSTRAINT chk_district_pollution CHECK (pollution <= 100)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE race_top (
  position_id TINYINT UNSIGNED NOT NULL PRIMARY KEY,
  username VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  best_seconds SMALLINT UNSIGNED NOT NULL,
  CONSTRAINT uq_race_top_username UNIQUE (username),
  CONSTRAINT chk_race_top_position CHECK (position_id BETWEEN 1 AND 10),
  CONSTRAINT chk_race_top_time CHECK (best_seconds BETWEEN 1 AND 300)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Completed only when all imported rows are committed in the SAME transaction.
CREATE TABLE legacy_imports (
  import_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  source_sha256 CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  snapshot_files INT UNSIGNED NOT NULL,
  accounts_count INT UNSIGNED NOT NULL,
  finished_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT uq_legacy_import_source UNIQUE(source_sha256)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
