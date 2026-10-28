-- GOV-00: player <-> state treasury atomic settlement; apply after 017 ONCE.
-- Requires MySQL 8.0.16+ or MariaDB 10.6+ and CREATE ROUTINE privileges.
-- DO NOT modify already applied migration 017. No seeded money, no automatic payments.
-- All monetary operations must be executed under a single InnoDB transaction.

CREATE TABLE gov_transfers (
  transfer_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  account_id BIGINT UNSIGNED NOT NULL,
  treasury_delta BIGINT NOT NULL,
  source VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  account_balance_after BIGINT UNSIGNED NOT NULL,
  treasury_balance_after BIGINT UNSIGNED NOT NULL,
  account_row_version_after BIGINT UNSIGNED NOT NULL,
  treasury_row_version_after INT UNSIGNED NOT NULL,
  account_txn_serial_after BIGINT UNSIGNED NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uq_gov_transfer_event (event_key),
  KEY idx_gov_transfer_account_recent (account_id, transfer_id),
  CONSTRAINT fk_gov_transfer_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_gov_transfer_delta CHECK
    (treasury_delta BETWEEN -1000000000 AND 1000000000 AND treasury_delta <> 0),
  CONSTRAINT chk_gov_transfer_account_balance CHECK (account_balance_after <= 1000000000),
  CONSTRAINT chk_gov_transfer_treasury_balance CHECK (treasury_balance_after <= 2000000000)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DELIMITER $$
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
         minimum_reserve,max_transaction
    INTO v_treasury,v_treasury_version,v_treasury_enabled,
         v_income_enabled,v_expense_enabled,v_minimum_reserve,v_max_transaction
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
      IF v_expense_enabled <> 1 OR v_treasury < v_minimum_reserve
         OR v_treasury - v_minimum_reserve < -p_treasury_delta
         OR v_account > 1000000000 + p_treasury_delta THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'gov transfer debit rejected';
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
DELIMITER ;

INSERT INTO schema_migrations(version) VALUES (18);
