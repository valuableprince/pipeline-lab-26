-- JOB-TRUCK-04. Apply ONCE after v10, BEFORE updating the server AMX.
-- InnoDB transactions atomically preserve loaded cargo and completed payments.
CREATE TABLE municipal_truck_cargo (
  bay_id TINYINT UNSIGNED NOT NULL PRIMARY KEY,
  account_id BIGINT UNSIGNED NULL,
  cargo_units SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  pending_pay INT UNSIGNED NOT NULL DEFAULT 0,
  pending_xp SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  revision INT UNSIGNED NOT NULL DEFAULT 0,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  CONSTRAINT fk_mtc_account FOREIGN KEY (account_id) REFERENCES accounts(account_id),
  CONSTRAINT chk_mtc_bay CHECK (bay_id BETWEEN 0 AND 1),
  CONSTRAINT chk_mtc_units CHECK (cargo_units BETWEEN 0 AND 200),
  CONSTRAINT chk_mtc_owner CHECK (
    (cargo_units=0 AND account_id IS NULL AND pending_pay=0 AND pending_xp=0)
    OR (cargo_units>0 AND account_id IS NOT NULL AND pending_pay>0 AND pending_xp=cargo_units)
  )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE municipal_driver_profiles (
  account_id BIGINT UNSIGNED NOT NULL PRIMARY KEY,
  experience INT UNSIGNED NOT NULL DEFAULT 0,
  delivered_units INT UNSIGNED NOT NULL DEFAULT 0,
  total_earned BIGINT UNSIGNED NOT NULL DEFAULT 0,
  completed_runs INT UNSIGNED NOT NULL DEFAULT 0,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  CONSTRAINT fk_mdp_account FOREIGN KEY (account_id) REFERENCES accounts(account_id)
    ON DELETE CASCADE,
  CONSTRAINT chk_mdp_experience CHECK (experience=delivered_units)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE municipal_truck_deliveries (
  delivery_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  bay_id TINYINT UNSIGNED NOT NULL,
  account_id BIGINT UNSIGNED NOT NULL,
  cargo_units SMALLINT UNSIGNED NOT NULL,
  salary INT UNSIGNED NOT NULL,
  experience SMALLINT UNSIGNED NOT NULL,
  completed_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT fk_mtd_account FOREIGN KEY (account_id) REFERENCES accounts(account_id),
  CONSTRAINT chk_mtd_units CHECK (cargo_units BETWEEN 1 AND 200),
  CONSTRAINT chk_mtd_experience CHECK (experience=cargo_units)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO municipal_truck_cargo (bay_id) VALUES (0),(1);
INSERT INTO schema_migrations (version) VALUES (11);
