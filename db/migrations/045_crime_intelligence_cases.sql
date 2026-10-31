-- CRIME-03.5: optional 30-minute broker intelligence from completed tenders.
-- Deploy after 044 with a stopped server and verified database backup.
CREATE TABLE crime_intelligence_cases (
  contract_id INT UNSIGNED NOT NULL PRIMARY KEY,
  puzzle_code TINYINT UNSIGNED NOT NULL,
  attempts SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  next_attempt_at INT UNSIGNED NOT NULL DEFAULT 0,
  solved_at INT UNSIGNED NOT NULL DEFAULT 0,
  decision TINYINT UNSIGNED NOT NULL DEFAULT 0,
  closed_at INT UNSIGNED NOT NULL DEFAULT 0,
  expires_at INT UNSIGNED NOT NULL,
  KEY idx_crime_intel_expires (expires_at),
  CONSTRAINT fk_crime_intel_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_intel_puzzle CHECK (puzzle_code BETWEEN 1 AND 3),
  CONSTRAINT chk_crime_intel_decision CHECK (decision IN(0,1,2)),
  CONSTRAINT chk_crime_intel_closed CHECK
    ((decision=0 AND closed_at=0) OR
     (decision IN(1,2) AND closed_at>0 AND solved_at>0)),
  CONSTRAINT chk_crime_intel_deadline CHECK (expires_at>0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Restore opportunities for recent tenders completed before deployment.
-- Old completed tenders do not gain an indefinite or renewed reward window.
INSERT INTO crime_intelligence_cases(contract_id,puzzle_code,expires_at)
  SELECT c.contract_id,1+MOD(c.contract_id,3),c.finished_at+1800
  FROM crime_contracts c
  JOIN crime_contract_rewards r ON r.contract_id=c.contract_id
  WHERE c.mode_id=3 AND c.state=3
    AND c.finished_at>UNIX_TIMESTAMP()-1800;

INSERT INTO schema_migrations(version) VALUES (45);
