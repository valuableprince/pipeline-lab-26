-- CRIME-02.7 | local NPC heat, risk and trust-independent solo recovery.
-- Migration is incremental and must be applied only once after 037.
-- heat 0..100 is per account AND per contact; no other factions are affected.
ALTER TABLE crime_contact_progress
  ADD COLUMN heat TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD CONSTRAINT chk_crime_contact_heat CHECK (heat BETWEEN 0 AND 100);

ALTER TABLE crime_contracts
  ADD COLUMN risk_delta SMALLINT NOT NULL DEFAULT 0,
  ADD CONSTRAINT chk_crime_contract_risk CHECK (risk_delta BETWEEN -12 AND 20);

CREATE TABLE crime_contact_heat_events (
  contract_id INT UNSIGNED NOT NULL PRIMARY KEY,
  account_id BIGINT UNSIGNED NOT NULL,
  contact_id TINYINT UNSIGNED NOT NULL,
  heat_delta SMALLINT NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  KEY idx_crime_heat_contact (account_id,contact_id),
  CONSTRAINT fk_crime_heat_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_heat_progress FOREIGN KEY (account_id,contact_id)
    REFERENCES crime_contact_progress(account_id,contact_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_heat_delta CHECK (heat_delta BETWEEN -12 AND 20)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- History before this migration has no heat events: all existing progress starts at zero.
-- Only future successful, settled contracts adjust heat, exactly once in a SQL transaction.
INSERT INTO schema_migrations (version) VALUES (38);
