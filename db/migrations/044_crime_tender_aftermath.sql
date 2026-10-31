-- CRIME-03.4: durable branch-dependent follow-up after the incident.
-- Apply only after 043 with the server stopped and a verified backup.
CREATE TABLE crime_tender_aftermath (
  contract_id INT UNSIGNED NOT NULL PRIMARY KEY,
  choice TINYINT UNSIGNED NOT NULL,
  state TINYINT UNSIGNED NOT NULL DEFAULT 1,
  finished_at INT UNSIGNED NOT NULL DEFAULT 0,
  CONSTRAINT fk_tender_aftermath_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT chk_tender_aftermath_choice CHECK (choice IN (1,2)),
  CONSTRAINT chk_tender_aftermath_state CHECK
    ((state=1 AND finished_at=0) OR (state=2 AND finished_at>0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Players whose incidents were completed before this deployment should
-- finish the new stage, rather than bypassing the new payout gate.
INSERT INTO crime_tender_aftermath(contract_id,choice)
  SELECT c.contract_id,i.choice FROM crime_contracts c
  JOIN crime_tender_incidents i ON i.contract_id=c.contract_id
  WHERE c.mode_id=3 AND c.state IN (1,2)
    AND i.choice IN (1,2) AND i.resolved_at>0;

INSERT INTO schema_migrations(version) VALUES (44);
