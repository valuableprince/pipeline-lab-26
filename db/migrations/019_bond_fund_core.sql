-- GOV-01: protected bond insurance reserve in the single state treasury.
-- Apply ONCE after 018 while the game server is stopped. MySQL 8.0.16+/MariaDB 10.6+.
-- No seeded cash, no automatic taxes/premiums, no insurance payout implementation.
-- Existing 017/018 migration files remain UNCHANGED.

ALTER TABLE treasury_accounts
  ADD COLUMN bond_fund_reserved BIGINT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN bond_fund_committed BIGINT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN bond_fund_enabled TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN bond_fund_coverage_bps SMALLINT UNSIGNED NOT NULL DEFAULT 2000,
  ADD COLUMN bond_fund_premium_bps SMALLINT UNSIGNED NOT NULL DEFAULT 200,
  ADD COLUMN bond_fund_utilization_bps SMALLINT UNSIGNED NOT NULL DEFAULT 8000,
  ADD COLUMN bond_fund_tax_allocation_bps SMALLINT UNSIGNED NOT NULL DEFAULT 500,
  ADD COLUMN bond_fund_grace_seconds INT UNSIGNED NOT NULL DEFAULT 86400,
  ADD COLUMN bond_fund_config_version INT UNSIGNED NOT NULL DEFAULT 0,
  ADD CONSTRAINT chk_bond_fund_balance CHECK (bond_fund_reserved <= balance),
  ADD CONSTRAINT chk_bond_fund_capacity CHECK
    (bond_fund_committed * 10000 <= bond_fund_reserved * bond_fund_utilization_bps),
  ADD CONSTRAINT chk_bond_fund_policy CHECK
    (bond_fund_enabled IN (0,1) AND bond_fund_coverage_bps BETWEEN 1 AND 10000
     AND bond_fund_premium_bps <= 10000
     AND bond_fund_utilization_bps BETWEEN 1 AND 10000
     AND bond_fund_tax_allocation_bps <= 10000
     AND bond_fund_grace_seconds BETWEEN 60 AND 2592000
     AND bond_fund_config_version <= 2147483646);

-- Append-only financial event log, independent from cash ledger.
-- reserve_delta represents reclassification of EXISTING treasury money, not a cash transfer.
CREATE TABLE bond_fund_ledger (
  entry_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  operation VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  reference_code VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  reserve_delta BIGINT NOT NULL DEFAULT 0,
  commitment_delta BIGINT NOT NULL DEFAULT 0,
  reserved_after BIGINT UNSIGNED NOT NULL,
  committed_after BIGINT UNSIGNED NOT NULL,
  treasury_row_version_after INT UNSIGNED NOT NULL,
  actor_account_id BIGINT UNSIGNED NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uq_bond_fund_event (event_key),
  KEY idx_bond_fund_actor (actor_account_id),
  CONSTRAINT fk_bond_fund_actor FOREIGN KEY (actor_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_bond_fund_event_amount CHECK
    ((reserve_delta <> 0 AND commitment_delta = 0)
     OR (reserve_delta = 0 AND commitment_delta <> 0)),
  CONSTRAINT chk_bond_fund_event_operation CHECK
    (operation IN ('CAPITALIZE','COMMIT'))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Reservation for a future issue: no bonds exist at GOV-01 yet.
-- Cannot release or pay guarantees before GOV-02/03 implements verified holdings/claims.
CREATE TABLE bond_fund_guarantees (
  issue_code VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
  max_exposure BIGINT UNSIGNED NOT NULL,
  event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uq_bond_guarantee_event (event_key),
  CONSTRAINT chk_bond_guarantee_exposure CHECK (max_exposure BETWEEN 1 AND 1000000000)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Replace all debit entrypoints in the SAME migration. No nested START TRANSACTION:
-- each of the replaced procedures remains its own top-level transaction.
-- Migration must run during maintenance. Mark schema version only after both succeed.
DELIMITER $$
DROP PROCEDURE IF EXISTS treasury_post$$
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
  DECLARE v_fund_reserved BIGINT UNSIGNED;
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
         minimum_reserve,max_transaction,bond_fund_reserved
    INTO v_balance,v_enabled,v_version,v_income,v_expense,v_reserve,v_max_transaction,v_fund_reserved
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
      IF v_balance < v_reserve + v_fund_reserved THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'treasury protected reserve';
      END IF;
      IF v_balance - v_reserve - v_fund_reserved < -p_delta THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'treasury insufficient unreserved funds';
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
DROP PROCEDURE IF EXISTS gov_transfer_post$$
CREATE PROCEDURE gov_transfer_post(
  IN p_event_key VARCHAR(64),
  IN p_account_id BIGINT UNSIGNED,
  IN p_treasury_delta INT,
  IN p_source VARCHAR(32),
  IN p_expected_account_version BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
  DECLARE v_treasury BIGINT UNSIGNED DEFAULT NULL;
  DECLARE v_treasury_version INT UNSIGNED;
  DECLARE v_treasury_enabled TINYINT UNSIGNED;
  DECLARE v_income_enabled TINYINT UNSIGNED;
  DECLARE v_expense_enabled TINYINT UNSIGNED;
  DECLARE v_minimum_reserve BIGINT UNSIGNED;
  DECLARE v_fund_reserved BIGINT UNSIGNED;
  DECLARE v_max_transaction BIGINT UNSIGNED;
  DECLARE v_account BIGINT UNSIGNED DEFAULT NULL;
  DECLARE v_account_version BIGINT UNSIGNED;
  DECLARE v_account_serial BIGINT UNSIGNED;
  DECLARE v_record_id BIGINT UNSIGNED DEFAULT NULL;
  DECLARE v_existing_account BIGINT UNSIGNED;
  DECLARE v_existing_delta BIGINT;
  DECLARE v_existing_source VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin;
  DECLARE v_new_treasury BIGINT;
  DECLARE v_new_account BIGINT;
  DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;

  DECLARE EXIT HANDLER FOR SQLEXCEPTION
  BEGIN
    ROLLBACK;
    RESIGNAL;
  END;

  IF p_account_id IS NULL OR p_account_id = 0
     OR p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
     OR LEFT(BINARY p_event_key, 6) <> BINARY 'gov00:'
     OR p_event_key NOT REGEXP '^gov00:[A-Za-z0-9_:/.-]+$'
     OR p_source IS NULL OR CHAR_LENGTH(p_source) NOT BETWEEN 1 AND 32
     OR p_source NOT REGEXP '^[A-Za-z0-9_:/.-]+$'
     OR p_treasury_delta IS NULL OR p_treasury_delta = 0
     OR ABS(p_treasury_delta) > 1000000000
     OR p_expected_account_version IS NULL
     OR p_expected_account_version > 2147483646 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'invalid gov transfer';
  END IF;

  START TRANSACTION;
  -- Fixed lock order shared by all GOV-00 requests: treasury then account.
  SELECT balance,row_version,enabled,income_enabled,expense_enabled,
         minimum_reserve,max_transaction,bond_fund_reserved
    INTO v_treasury,v_treasury_version,v_treasury_enabled,
         v_income_enabled,v_expense_enabled,v_minimum_reserve,v_max_transaction,v_fund_reserved
    FROM treasury_accounts WHERE code='state' FOR UPDATE;

  SELECT balance,row_version,txn_serial
    INTO v_account,v_account_version,v_account_serial
    FROM accounts WHERE account_id=p_account_id FOR UPDATE;

  IF v_treasury IS NULL OR v_account IS NULL THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'transfer account missing';
  END IF;

  -- Check replay after taking both locks; retries never modify money.
  SELECT transfer_id,account_id,treasury_delta,source
    INTO v_record_id,v_existing_account,v_existing_delta,v_existing_source
    FROM gov_transfers WHERE event_key=p_event_key LIMIT 1;

  IF v_record_id IS NOT NULL THEN
    IF v_existing_account <> p_account_id OR v_existing_delta <> p_treasury_delta
       OR BINARY v_existing_source <> BINARY p_source THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'gov transfer event collision';
    END IF;
    SET v_replayed = 1;
  ELSE
    IF v_account_version <> p_expected_account_version
       OR v_account_version >= 2147483646
       OR v_account_serial >= 1000000000
       OR v_treasury_version >= 2147483646 THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'gov transfer account version conflict';
    END IF;

    IF v_treasury_enabled <> 1 OR ABS(p_treasury_delta) > v_max_transaction THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'gov transfer treasury policy rejected';
    END IF;

    IF p_treasury_delta > 0 THEN
      -- Player -> treasury. 1 billion is the account cap, 2 billion treasury cap.
      IF v_income_enabled <> 1 OR v_account < p_treasury_delta
         OR v_treasury > 2000000000 - p_treasury_delta THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'gov transfer credit rejected';
      END IF;
    ELSE
      -- Treasury -> player; protect the minimum treasury reserve.
      IF v_expense_enabled <> 1
         OR v_treasury < v_minimum_reserve + v_fund_reserved
         OR v_account > 1000000000 + p_treasury_delta THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'gov transfer debit rejected';
      END IF;
      IF v_treasury - v_minimum_reserve - v_fund_reserved < -p_treasury_delta THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'gov transfer protected reserve';
      END IF;
    END IF;

    SET v_new_treasury = CAST(v_treasury AS SIGNED) + p_treasury_delta;
    SET v_new_account = CAST(v_account AS SIGNED) - p_treasury_delta;

    UPDATE treasury_accounts
      SET balance=v_new_treasury,row_version=row_version+1 WHERE code='state';
    UPDATE accounts
      SET balance=v_new_account,txn_serial=txn_serial+1,
          row_version=row_version+1 WHERE account_id=p_account_id;
    INSERT INTO treasury_ledger
      (event_key,amount_delta,balance_after,source,actor_account_id,note)
      VALUES(p_event_key,p_treasury_delta,v_new_treasury,
             'gov_transfer',p_account_id,p_source);
    INSERT INTO gov_transfers
      (event_key,account_id,treasury_delta,source,account_balance_after,
       treasury_balance_after,account_row_version_after,
       treasury_row_version_after,account_txn_serial_after)
      VALUES(p_event_key,p_account_id,p_treasury_delta,p_source,v_new_account,
             v_new_treasury,v_account_version+1,v_treasury_version+1,v_account_serial+1);

    SET v_treasury = v_new_treasury;
    SET v_account = v_new_account;
    SET v_treasury_version = v_treasury_version + 1;
    SET v_account_version = v_account_version + 1;
    SET v_account_serial = v_account_serial + 1;
  END IF;

  COMMIT;
  -- Current values, including on an idempotent replay.
  SELECT v_account AS account_balance,v_treasury AS treasury_balance,
         v_account_version AS account_row_version,
         v_treasury_version AS treasury_row_version,
         v_account_serial AS account_txn_serial,v_replayed AS replayed;
END$$

CREATE PROCEDURE bond_fund_capitalize(
  IN p_event_key VARCHAR(64),
  IN p_amount INT,
  IN p_actor_account_id BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
  DECLARE v_balance BIGINT UNSIGNED DEFAULT NULL;
  DECLARE v_reserved BIGINT UNSIGNED;
  DECLARE v_committed BIGINT UNSIGNED;
  DECLARE v_minimum_reserve BIGINT UNSIGNED;
  DECLARE v_row_version INT UNSIGNED;
  DECLARE v_id BIGINT UNSIGNED DEFAULT NULL;
  DECLARE v_operation VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin;
  DECLARE v_delta BIGINT;
  DECLARE v_existing_actor BIGINT UNSIGNED;
  DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;

  DECLARE EXIT HANDLER FOR SQLEXCEPTION
  BEGIN
    ROLLBACK;
    RESIGNAL;
  END;

  IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 6 AND 64
     OR LEFT(BINARY p_event_key,5) <> BINARY 'fund:'
     OR p_event_key NOT REGEXP '^fund:[A-Za-z0-9_:/.-]+$'
     OR p_amount IS NULL OR p_amount <= 0 OR p_amount > 1000000000
     OR p_actor_account_id = 0 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'invalid fund capitalization';
  END IF;

  START TRANSACTION;
  SELECT balance,bond_fund_reserved,bond_fund_committed,minimum_reserve,row_version
    INTO v_balance,v_reserved,v_committed,v_minimum_reserve,v_row_version
    FROM treasury_accounts WHERE code='state' FOR UPDATE;

  IF v_balance IS NULL THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'treasury missing';
  END IF;

  SELECT entry_id,operation,reserve_delta,actor_account_id
    INTO v_id,v_operation,v_delta,v_existing_actor
    FROM bond_fund_ledger WHERE event_key=p_event_key LIMIT 1;

  IF v_id IS NOT NULL THEN
    IF v_operation <> 'CAPITALIZE' OR v_delta <> p_amount
       OR NOT (v_existing_actor <=> p_actor_account_id) THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'fund event collision';
    END IF;
    SET v_replayed = 1;
  ELSE
    IF v_row_version >= 2147483646 OR v_reserved > v_balance
       OR v_balance < v_minimum_reserve + v_reserved
       OR v_balance - v_minimum_reserve - v_reserved < p_amount THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'fund capitalization exceeds free budget';
    END IF;
    UPDATE treasury_accounts
      SET bond_fund_reserved=bond_fund_reserved+p_amount,row_version=row_version+1
      WHERE code='state';
    SET v_reserved=v_reserved+p_amount;
    SET v_row_version=v_row_version+1;
    INSERT INTO bond_fund_ledger
      (event_key,operation,reference_code,reserve_delta,commitment_delta,
       reserved_after,committed_after,treasury_row_version_after,actor_account_id)
      VALUES(p_event_key,'CAPITALIZE',NULL,p_amount,0,
             v_reserved,v_committed,v_row_version,p_actor_account_id);
  END IF;
  COMMIT;
  SELECT v_balance AS balance,v_reserved AS fund_reserved,
         v_committed AS fund_committed,v_row_version AS row_version,
         v_replayed AS replayed;
END$$

CREATE PROCEDURE bond_fund_commit(
  IN p_event_key VARCHAR(64),
  IN p_issue_code VARCHAR(64),
  IN p_exposure INT,
  IN p_actor_account_id BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
  DECLARE v_balance BIGINT UNSIGNED DEFAULT NULL;
  DECLARE v_reserved BIGINT UNSIGNED;
  DECLARE v_committed BIGINT UNSIGNED;
  DECLARE v_row_version INT UNSIGNED;
  DECLARE v_enabled TINYINT UNSIGNED;
  DECLARE v_utilization SMALLINT UNSIGNED;
  DECLARE v_id BIGINT UNSIGNED DEFAULT NULL;
  DECLARE v_operation VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin;
  DECLARE v_ref VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin;
  DECLARE v_delta BIGINT;
  DECLARE v_existing_actor BIGINT UNSIGNED;
  DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;

  DECLARE EXIT HANDLER FOR SQLEXCEPTION
  BEGIN
    ROLLBACK;
    RESIGNAL;
  END;

  IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 6 AND 64
     OR LEFT(BINARY p_event_key,5) <> BINARY 'fund:'
     OR p_event_key NOT REGEXP '^fund:[A-Za-z0-9_:/.-]+$'
     OR p_issue_code IS NULL OR CHAR_LENGTH(p_issue_code) NOT BETWEEN 1 AND 64
     OR p_issue_code NOT REGEXP '^[A-Za-z0-9_:/.-]+$'
     OR p_exposure IS NULL OR p_exposure <= 0 OR p_exposure > 1000000000
     OR p_actor_account_id = 0 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'invalid fund commitment';
  END IF;

  START TRANSACTION;
  SELECT balance,bond_fund_reserved,bond_fund_committed,row_version,
         bond_fund_enabled,bond_fund_utilization_bps
    INTO v_balance,v_reserved,v_committed,v_row_version,v_enabled,v_utilization
    FROM treasury_accounts WHERE code='state' FOR UPDATE;

  IF v_balance IS NULL THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'treasury missing';
  END IF;

  SELECT entry_id,operation,reference_code,commitment_delta,actor_account_id
    INTO v_id,v_operation,v_ref,v_delta,v_existing_actor
    FROM bond_fund_ledger WHERE event_key=p_event_key LIMIT 1;

  IF v_id IS NOT NULL THEN
    IF v_operation <> 'COMMIT' OR NOT (BINARY v_ref <=> BINARY p_issue_code)
       OR v_delta <> p_exposure
       OR NOT (v_existing_actor <=> p_actor_account_id) THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'fund event collision';
    END IF;
    SET v_replayed = 1;
  ELSE
    IF v_enabled <> 1 OR v_row_version >= 2147483646
       OR (v_committed + p_exposure) * 10000 > v_reserved * v_utilization THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'fund guarantee capacity exceeded';
    END IF;

    INSERT INTO bond_fund_guarantees(issue_code,max_exposure,event_key)
      VALUES(p_issue_code,p_exposure,p_event_key);
    UPDATE treasury_accounts
      SET bond_fund_committed=bond_fund_committed+p_exposure,
          row_version=row_version+1 WHERE code='state';
    SET v_committed=v_committed+p_exposure;
    SET v_row_version=v_row_version+1;
    INSERT INTO bond_fund_ledger
      (event_key,operation,reference_code,reserve_delta,commitment_delta,
       reserved_after,committed_after,treasury_row_version_after,actor_account_id)
      VALUES(p_event_key,'COMMIT',p_issue_code,0,p_exposure,
             v_reserved,v_committed,v_row_version,p_actor_account_id);
  END IF;
  COMMIT;
  SELECT v_balance AS balance,v_reserved AS fund_reserved,
         v_committed AS fund_committed,v_row_version AS row_version,
         v_replayed AS replayed;
END$$
DELIMITER ;

INSERT INTO schema_migrations(version) VALUES (19);
