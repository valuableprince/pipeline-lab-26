-- PASSPORT-01. Dedicated document serial, separated from internal accounts.account_id.
-- Apply AFTER 030 with a DBA backup before deploying the corresponding Pawn.
-- Each existing account receives a passport, keeping the original issue date.
CREATE TABLE player_passports (
    passport_id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    account_id BIGINT UNSIGNED NOT NULL,
    birth_date DATE DEFAULT NULL,
    issued_on DATE NOT NULL,
    UNIQUE KEY uq_player_passports_account (account_id),
    CONSTRAINT fk_player_passports_account FOREIGN KEY (account_id)
      REFERENCES accounts(account_id) ON DELETE RESTRICT,
    CONSTRAINT chk_passport_birth CHECK (birth_date IS NULL OR
      birth_date BETWEEN '1900-01-01' AND '2100-12-31')
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 AUTO_INCREMENT=500001;

-- Deterministic backfill; no change to balance, experience or authorization.
INSERT INTO player_passports (account_id, issued_on)
SELECT account_id, DATE(created_at) FROM accounts ORDER BY account_id;

INSERT INTO schema_migrations (version) VALUES (31);
