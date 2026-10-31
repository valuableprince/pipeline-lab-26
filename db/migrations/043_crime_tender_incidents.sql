-- CRIME-03.3: durable NPC road encounters for tender deliveries.
-- Must be applied after 042, with the server stopped and a verified backup.
CREATE TABLE crime_tender_incidents (
  contract_id INT UNSIGNED NOT NULL PRIMARY KEY,
  incident_code TINYINT UNSIGNED NOT NULL,
  choice TINYINT UNSIGNED NOT NULL DEFAULT 0,
  resolved_at INT UNSIGNED NOT NULL DEFAULT 0,
  CONSTRAINT fk_tender_incident_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT chk_tender_incident_code CHECK (incident_code BETWEEN 1 AND 3),
  CONSTRAINT chk_tender_incident_choice CHECK (choice BETWEEN 0 AND 2),
  CONSTRAINT chk_tender_incident_resolution CHECK
    ((choice=0 AND resolved_at=0) OR (choice IN(1,2) AND resolved_at>0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Protect in-progress mode=3 contracts awarded before the 043 deployment.
-- Historical completed contracts need no retroactive incident or payout.
INSERT INTO crime_tender_incidents(contract_id,incident_code)
  SELECT contract_id,1+MOD(contract_id,3)
  FROM crime_contracts
  WHERE mode_id=3 AND state IN(1,2);

INSERT INTO schema_migrations(version) VALUES (43);
