-- CRIME-02.1 | solo contracts core. Apply once after 031, with server stopped.
-- MySQL 8.0.16+ / MariaDB 10.6+. All tables InnoDB/utf8mb4.
-- State: 1=ACCEPTED, 2=PICKED_UP, 3=COMPLETED, 4=EXPIRED, 5=ABORTED.
-- Database owns contract lifecycle, payout and unique active contract per account.

CREATE TABLE crime_contract_templates (
  template_id SMALLINT UNSIGNED NOT NULL PRIMARY KEY,
  code VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  title VARCHAR(64) NOT NULL,
  mode_id TINYINT UNSIGNED NOT NULL DEFAULT 1,
  reward_amount INT UNSIGNED NOT NULL,
  duration_seconds INT UNSIGNED NOT NULL,
  pickup_x DECIMAL(10,3) NOT NULL,
  pickup_y DECIMAL(10,3) NOT NULL,
  pickup_z DECIMAL(10,3) NOT NULL,
  delivery_x DECIMAL(10,3) NOT NULL,
  delivery_y DECIMAL(10,3) NOT NULL,
  delivery_z DECIMAL(10,3) NOT NULL,
  is_enabled TINYINT UNSIGNED NOT NULL DEFAULT 1,
  CONSTRAINT uq_crime_template_code UNIQUE (code),
  CONSTRAINT chk_crime_template_mode CHECK (mode_id IN (1,2,3)),
  CONSTRAINT chk_crime_template_reward CHECK (reward_amount BETWEEN 1 AND 50000),
  CONSTRAINT chk_crime_template_duration CHECK (duration_seconds BETWEEN 120 AND 7200),
  CONSTRAINT chk_crime_template_enabled CHECK (is_enabled IN (0,1))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE crime_contracts (
  contract_id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  template_id SMALLINT UNSIGNED NOT NULL,
  account_id BIGINT UNSIGNED NOT NULL,
  faction_id TINYINT UNSIGNED NOT NULL,
  mode_id TINYINT UNSIGNED NOT NULL,
  state TINYINT UNSIGNED NOT NULL DEFAULT 1,
  reward_amount INT UNSIGNED NOT NULL,
  started_at INT UNSIGNED NOT NULL,
  expires_at INT UNSIGNED NOT NULL,
  picked_up_at INT UNSIGNED NOT NULL DEFAULT 0,
  finished_at INT UNSIGNED NOT NULL DEFAULT 0,
  active_account_id BIGINT UNSIGNED GENERATED ALWAYS AS
    (CASE WHEN state IN (1,2) THEN account_id ELSE NULL END) STORED,
  UNIQUE KEY uq_crime_one_active_contract (active_account_id),
  KEY idx_crime_contract_account (account_id, contract_id),
  KEY idx_crime_contract_faction (faction_id, started_at),
  CONSTRAINT fk_crime_contract_template FOREIGN KEY (template_id)
    REFERENCES crime_contract_templates(template_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_contract_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_contract_faction FOREIGN KEY (faction_id)
    REFERENCES factions(faction_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_contract_state CHECK (state BETWEEN 1 AND 5),
  CONSTRAINT chk_crime_contract_mode CHECK (mode_id IN (1,2,3)),
  CONSTRAINT chk_crime_contract_reward CHECK (reward_amount BETWEEN 1 AND 50000),
  CONSTRAINT chk_crime_contract_clock CHECK (expires_at > started_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE crime_contract_events (
  event_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  contract_id INT UNSIGNED NOT NULL,
  actor_account_id BIGINT UNSIGNED NOT NULL,
  event_type VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uq_crime_contract_event (contract_id, event_type),
  KEY idx_crime_actor_events (actor_account_id, event_id),
  CONSTRAINT fk_crime_event_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_event_actor FOREIGN KEY (actor_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE crime_contract_rewards (
  contract_id INT UNSIGNED NOT NULL PRIMARY KEY,
  account_id BIGINT UNSIGNED NOT NULL,
  amount INT UNSIGNED NOT NULL,
  balance_after BIGINT UNSIGNED NOT NULL,
  row_version_after BIGINT UNSIGNED NOT NULL,
  txn_serial_after BIGINT UNSIGNED NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT fk_crime_reward_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_reward_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_reward_amount CHECK (amount BETWEEN 1 AND 50000),
  CONSTRAINT chk_crime_reward_balance CHECK (balance_after <= 1000000000)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Pilot noncompetitive solo operation: the route is a UAT location draft.
-- Its positions can be adjusted independently in the template table.
INSERT INTO crime_contract_templates
  (template_id,code,title,mode_id,reward_amount,duration_seconds,
   pickup_x,pickup_y,pickup_z,delivery_x,delivery_y,delivery_z)
VALUES
  (1,'SOLO_PORT_01','Ночной рейс',1,2000,1800,
   2765.000,-2410.000,13.500,2220.000,-2385.000,13.500);

INSERT INTO schema_migrations (version) VALUES (32);
