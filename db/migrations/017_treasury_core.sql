-- TREASURY-01. Apply once after 016. MySQL 8.0.16+ / MariaDB 10.6+.
-- Independent government treasury; no changes to players' account balances.
-- Keep runtime DML inside treasury_post: balance and ledger must commit atomically.

CREATE TABLE treasury_accounts (
  code VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
  balance BIGINT UNSIGNED NOT NULL DEFAULT 0,
  enabled TINYINT UNSIGNED NOT NULL DEFAULT 1,
  income_enabled TINYINT UNSIGNED NOT NULL DEFAULT 1,
  expense_enabled TINYINT UNSIGNED NOT NULL DEFAULT 1,
  minimum_reserve BIGINT UNSIGNED NOT NULL DEFAULT 0,
  max_transaction BIGINT UNSIGNED NOT NULL DEFAULT 1000000000,
  income_tax_basis_points SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  row_version INT UNSIGNED NOT NULL DEFAULT 0,
  config_version INT UNSIGNED NOT NULL DEFAULT 0,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  -- A CHECK constraint plus the primary key makes a second treasury impossible.
  CONSTRAINT chk_treasury_singleton CHECK (code = 'state'),
  CONSTRAINT chk_treasury_balance CHECK (balance <= 2000000000),
  CONSTRAINT chk_treasury_enabled CHECK (
    enabled IN (0,1) AND income_enabled IN (0,1) AND expense_enabled IN (0,1)
  ),
  CONSTRAINT chk_treasury_policy CHECK (
    minimum_reserve <= 2000000000 AND max_transaction BETWEEN 1 AND 1000000000
    AND income_tax_basis_points <= 10000
  ),
  CONSTRAINT chk_treasury_version CHECK (
    row_version <= 2147483646 AND config_version <= 2147483646
  )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE treasury_ledger (
  ledger_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  amount_delta BIGINT NOT NULL,
  balance_after BIGINT UNSIGNED NOT NULL,
  source VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  actor_account_id BIGINT UNSIGNED NULL,
  note VARCHAR(128) NOT NULL DEFAULT '',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uq_treasury_event (event_key),
  KEY idx_treasury_ledger_actor (actor_account_id),
  CONSTRAINT fk_treasury_ledger_actor FOREIGN KEY (actor_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_treasury_delta CHECK (amount_delta BETWEEN -1000000000 AND 1000000000 AND amount_delta <> 0),
  CONSTRAINT chk_treasury_balance_after CHECK (balance_after <= 2000000000)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO treasury_accounts(code,balance) VALUES ('state',0);

DELIMITER $$
CREATE PROCEDURE treasury_post(
  IN p_event_key VARCHAR(64),
  IN p_delta INT,
  IN p_source VARCHAR(32),
  IN p_actor_id BIGINT UNSIGNED,
  IN p_note VARCHAR(128)
)
SQL SECURITY INVOKER
BEGIN
  DECLARE v_balance BIGINT UNSIGNED DEFAULT NULL;
  DECLARE v_enabled TINYINT UNSIGNED;
  DECLARE v_income TINYINT UNSIGNED;
  DECLARE v_expense TINYINT UNSIGNED;
  DECLARE v_reserve BIGINT UNSIGNED;
  DECLARE v_max_transaction BIGINT UNSIGNED;
  DECLARE v_version INT UNSIGNED;
  DECLARE v_new_balance BIGINT;
  DECLARE v_existing_id BIGINT UNSIGNED DEFAULT NULL;
  DECLARE v_existing_delta BIGINT;
  DECLARE v_existing_source VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin;
  DECLARE v_existing_actor BIGINT UNSIGNED;
  DECLARE v_existing_note VARCHAR(128);
  DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;

  DECLARE EXIT HANDLER FOR SQLEXCEPTION
  BEGIN
    ROLLBACK;
    RESIGNAL;
  END;

  IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 1 AND 64
     OR p_source IS NULL OR CHAR_LENGTH(p_source) NOT BETWEEN 1 AND 32
     OR p_note IS NULL OR CHAR_LENGTH(p_note) > 128
     OR p_delta IS NULL OR p_delta = 0 OR ABS(p_delta) > 1000000000 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'invalid treasury operation';
  END IF;

  START TRANSACTION;
  SELECT balance,enabled,row_version,income_enabled,expense_enabled,
         minimum_reserve,max_transaction
    INTO v_balance,v_enabled,v_version,v_income,v_expense,v_reserve,v_max_transaction
    FROM treasury_accounts
    WHERE code='state' FOR UPDATE;

  IF v_balance IS NULL THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'treasury not initialized';
  END IF;

  -- Lock account first; replay checks are serialized even across server processes.
  SELECT ledger_id,amount_delta,source,actor_account_id,note
    INTO v_existing_id,v_existing_delta,v_existing_source,v_existing_actor,v_existing_note
    FROM treasury_ledger
    WHERE event_key=p_event_key LIMIT 1;

  IF v_existing_id IS NOT NULL THEN
    IF v_existing_delta <> p_delta OR v_existing_source <> p_source
       OR NOT (v_existing_actor <=> NULLIF(p_actor_id,0))
       OR BINARY v_existing_note <> BINARY p_note THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'treasury event key collision';
    END IF;
    SET v_replayed = 1;
  ELSE
    IF v_enabled <> 1 THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'treasury disabled';
    END IF;
    IF p_delta > 0 THEN
      IF v_income <> 1 OR p_delta > v_max_transaction
         OR v_balance > 2000000000 - p_delta THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'treasury credit rejected';
      END IF;
    ELSE
      IF v_expense <> 1 OR -p_delta > v_max_transaction
         OR v_balance < v_reserve THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'treasury debit rejected';
      END IF;
      IF v_balance - v_reserve < -p_delta THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'treasury insufficient funds';
      END IF;
    END IF;
    IF v_version >= 2147483646 THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'treasury version limit';
    END IF;

    SET v_new_balance = CAST(v_balance AS SIGNED) + p_delta;
    UPDATE treasury_accounts
      SET balance=v_new_balance, row_version=row_version+1
      WHERE code='state';
    INSERT INTO treasury_ledger(event_key,amount_delta,balance_after,source,actor_account_id,note)
      VALUES (p_event_key,p_delta,v_new_balance,p_source,NULLIF(p_actor_id,0),p_note);
    SET v_balance = v_new_balance;
    SET v_version = v_version + 1;
  END IF;

  COMMIT;
  -- Exactly one result set; replay reports the CURRENT balance, not historic balance_after.
  SELECT v_balance AS balance_after, v_version AS row_version, v_replayed AS replayed;
END$$
DELIMITER ;

INSERT INTO schema_migrations(version) VALUES (17);
