-- GOV-03: atomic bond purchase; maturity and insurance allocations are escrowed
-- inside the SINGLE treasury and collected through account-version-checked SQL.
-- Apply exactly once after 020. No artificial cash, no direct offline wallet writes.
-- Migration must run while the server is stopped; DDL is not fully transactional.

ALTER TABLE treasury_accounts
  ADD COLUMN bond_payout_reserved BIGINT UNSIGNED NOT NULL DEFAULT 0,
  ADD CONSTRAINT chk_bond_payout_reserve CHECK
    (bond_fund_reserved + bond_payout_reserved <= balance);

ALTER TABLE government_bond_issues
  ADD COLUMN maturity_event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  ADD COLUMN insurance_event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  ADD CONSTRAINT uq_bond_maturity_event UNIQUE (maturity_event_key),
  ADD CONSTRAINT uq_bond_insurance_event UNIQUE (insurance_event_key);

ALTER TABLE bond_fund_ledger
  DROP CONSTRAINT chk_bond_fund_event_amount,
  DROP CONSTRAINT chk_bond_fund_event_operation,
  ADD CONSTRAINT chk_bond_fund_event_amount CHECK
    (reserve_delta <> 0 OR commitment_delta <> 0),
  ADD CONSTRAINT chk_bond_fund_event_operation CHECK
    (operation IN ('CAPITALIZE','COMMIT','RELEASE','PREMIUM','INSURANCE_ALLOCATE'));

CREATE TABLE government_bond_orders (
  order_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  issue_id BIGINT UNSIGNED NOT NULL,
  account_id BIGINT UNSIGNED NOT NULL,
  quantity INT UNSIGNED NOT NULL,
  amount_paid BIGINT UNSIGNED NOT NULL,
  fund_premium BIGINT UNSIGNED NOT NULL,
  account_balance_after BIGINT UNSIGNED NOT NULL,
  account_version_after INT UNSIGNED NOT NULL,
  account_serial_after BIGINT UNSIGNED NOT NULL,
  treasury_balance_after BIGINT UNSIGNED NOT NULL,
  treasury_version_after INT UNSIGNED NOT NULL,
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  UNIQUE KEY uq_gov_bond_order_event (event_key),
  KEY idx_gov_bond_order_account (account_id,order_id),
  CONSTRAINT fk_bond_order_issue FOREIGN KEY (issue_id)
    REFERENCES government_bond_issues(issue_id) ON DELETE RESTRICT,
  CONSTRAINT fk_bond_order_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_gov_bond_order_price CHECK
    (quantity BETWEEN 1 AND 1000000 AND amount_paid BETWEEN 1 AND 1000000000
     AND fund_premium <= amount_paid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Claims are frozen at maturity and may later gain an insurance entitlement.
-- "awarded" is RESERVED in treasury, NOT yet placed in a wallet.
CREATE TABLE government_bond_claims (
  claim_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  issue_id BIGINT UNSIGNED NOT NULL,
  account_id BIGINT UNSIGNED NOT NULL,
  principal_due BIGINT UNSIGNED NOT NULL,
  interest_due BIGINT UNSIGNED NOT NULL,
  principal_budget_awarded BIGINT UNSIGNED NOT NULL DEFAULT 0,
  interest_budget_awarded BIGINT UNSIGNED NOT NULL DEFAULT 0,
  insurance_awarded BIGINT UNSIGNED NOT NULL DEFAULT 0,
  amount_collected BIGINT UNSIGNED NOT NULL DEFAULT 0,
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  UNIQUE KEY uq_bond_claim_owner (issue_id,account_id),
  KEY idx_bond_claim_owner (account_id,claim_id),
  CONSTRAINT fk_bond_claim_issue FOREIGN KEY (issue_id)
    REFERENCES government_bond_issues(issue_id) ON DELETE RESTRICT,
  CONSTRAINT fk_bond_claim_owner FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_bond_claim_amounts CHECK
    (principal_due > 0 AND principal_budget_awarded <= principal_due
     AND interest_budget_awarded <= interest_due
     AND insurance_awarded <= principal_due-principal_budget_awarded
     AND amount_collected <=
       principal_budget_awarded+interest_budget_awarded+insurance_awarded)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE government_bond_claim_receipts (
  receipt_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  claim_id BIGINT UNSIGNED NOT NULL,
  account_id BIGINT UNSIGNED NOT NULL,
  amount BIGINT UNSIGNED NOT NULL,
  account_balance_after BIGINT UNSIGNED NOT NULL,
  account_version_after INT UNSIGNED NOT NULL,
  account_serial_after BIGINT UNSIGNED NOT NULL,
  treasury_version_after INT UNSIGNED NOT NULL,
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  UNIQUE KEY uq_bond_receipt_event (event_key),
  KEY idx_bond_receipt_claim (claim_id,receipt_id),
  CONSTRAINT fk_bond_receipt_claim FOREIGN KEY (claim_id)
    REFERENCES government_bond_claims(claim_id) ON DELETE RESTRICT,
  CONSTRAINT fk_bond_receipt_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_bond_receipt_amount CHECK
    (amount BETWEEN 1 AND 1000000000 AND account_balance_after <= 1000000000)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE bond_payout_ledger (
  entry_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  operation VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  issue_id BIGINT UNSIGNED NOT NULL,
  reserved_delta BIGINT NOT NULL,
  reserved_after BIGINT UNSIGNED NOT NULL,
  treasury_row_version_after INT UNSIGNED NOT NULL,
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  UNIQUE KEY uq_bond_payout_event (event_key),
  KEY idx_bond_payout_issue (issue_id,entry_id),
  CONSTRAINT fk_bond_payout_issue FOREIGN KEY (issue_id)
    REFERENCES government_bond_issues(issue_id) ON DELETE RESTRICT,
  CONSTRAINT chk_bond_payout_event CHECK
    (operation IN ('MATURITY','INSURANCE','COLLECT') AND
     (reserved_delta <> 0 OR operation='MATURITY' OR operation='INSURANCE'))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DELIMITER $$
-- All ordinary withdrawal paths protect both reserves, even while players are offline.
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
  DECLARE v_payout_reserved BIGINT UNSIGNED;
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
         minimum_reserve,max_transaction,bond_fund_reserved,bond_payout_reserved
    INTO v_balance,v_enabled,v_version,v_income,v_expense,v_reserve,v_max_transaction,v_fund_reserved,v_payout_reserved
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
      IF v_balance < v_reserve + v_fund_reserved + v_payout_reserved THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'treasury protected reserve';
      END IF;
      IF v_balance - v_reserve - v_fund_reserved - v_payout_reserved < -p_delta THEN
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
  DECLARE v_payout_reserved BIGINT UNSIGNED;
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
         minimum_reserve,max_transaction,bond_fund_reserved,bond_payout_reserved
    INTO v_treasury,v_treasury_version,v_treasury_enabled,
         v_income_enabled,v_expense_enabled,v_minimum_reserve,v_max_transaction,v_fund_reserved,v_payout_reserved
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
         OR v_treasury < v_minimum_reserve + v_fund_reserved + v_payout_reserved
         OR v_account > 1000000000 + p_treasury_delta THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'gov transfer debit rejected';
      END IF;
      IF v_treasury - v_minimum_reserve - v_fund_reserved - v_payout_reserved < -p_treasury_delta THEN
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

CREATE PROCEDURE gov_bond_buy(
 IN p_event_key VARCHAR(64),
 IN p_issue_code VARCHAR(64),
 IN p_account_id BIGINT UNSIGNED,
 IN p_quantity INT,
 IN p_expected_version INT
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_balance BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_reserved BIGINT UNSIGNED;
 DECLARE v_payout BIGINT UNSIGNED;
 DECLARE v_committed BIGINT UNSIGNED;
 DECLARE v_treasury_version INT UNSIGNED;
 DECLARE v_treasury_enabled TINYINT UNSIGNED;
 DECLARE v_income_enabled TINYINT UNSIGNED;
 DECLARE v_max_transaction BIGINT UNSIGNED;
 DECLARE v_account_balance BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_account_version INT UNSIGNED;
 DECLARE v_serial BIGINT UNSIGNED;
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_status VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_start DATETIME;
 DECLARE v_end DATETIME;
 DECLARE v_face INT UNSIGNED;
 DECLARE v_max_qty INT UNSIGNED;
 DECLARE v_sold INT UNSIGNED;
 DECLARE v_premium_bps SMALLINT UNSIGNED;
 DECLARE v_existing_qty INT UNSIGNED DEFAULT 0;
 DECLARE v_payment BIGINT UNSIGNED;
 DECLARE v_premium BIGINT UNSIGNED;
 DECLARE v_order BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_order_issue BIGINT UNSIGNED;
 DECLARE v_order_account BIGINT UNSIGNED;
 DECLARE v_order_quantity INT UNSIGNED;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;

 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
    OR LEFT(BINARY p_event_key,5) <> BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_issue_code IS NULL OR CHAR_LENGTH(p_issue_code) NOT BETWEEN 1 AND 64
    OR p_issue_code NOT REGEXP '^[A-Za-z0-9_:/.-]+$'
    OR p_account_id IS NULL OR p_account_id=0
    OR p_quantity IS NULL OR p_quantity < 1 OR p_quantity > 1000000
    OR p_expected_version IS NULL OR p_expected_version < 0
    OR p_expected_version >= 2147483646 THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid bond purchase';
 END IF;
 START TRANSACTION;
 SELECT balance,bond_fund_reserved,bond_payout_reserved,bond_fund_committed,
        row_version,enabled,income_enabled,max_transaction
   INTO v_balance,v_reserved,v_payout,v_committed,v_treasury_version,
        v_treasury_enabled,v_income_enabled,v_max_transaction
   FROM treasury_accounts WHERE code='state' FOR UPDATE;
 SELECT balance,row_version,txn_serial
   INTO v_account_balance,v_account_version,v_serial
   FROM accounts WHERE account_id=p_account_id FOR UPDATE;
 SELECT issue_id,status,offer_start_at,offer_end_at,face_value,max_quantity,
        sold_quantity,premium_basis_points
   INTO v_issue,v_status,v_start,v_end,v_face,v_max_qty,v_sold,v_premium_bps
   FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE;
 IF v_balance IS NULL OR v_account_balance IS NULL OR v_issue IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond purchase account or issue missing';
 END IF;
 SELECT order_id,issue_id,account_id,quantity
   INTO v_order,v_order_issue,v_order_account,v_order_quantity
   FROM government_bond_orders WHERE event_key=p_event_key LIMIT 1;
 IF v_order IS NOT NULL THEN
   IF v_order_issue <> v_issue OR v_order_account <> p_account_id OR
      v_order_quantity <> p_quantity THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond purchase event collision';
   END IF;
   SET v_replayed=1;
 ELSE
   IF v_status <> 'OFFERING' OR v_start > UTC_TIMESTAMP()
      OR v_end <= UTC_TIMESTAMP() OR v_sold + p_quantity > v_max_qty
      OR v_account_version <> p_expected_version OR
      v_account_version >= 2147483646 OR v_serial >= 1000000000 OR
      v_treasury_version >= 2147483646 OR v_treasury_enabled <> 1 OR
      v_income_enabled <> 1 OR v_premium_bps IS NULL THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond purchase blocked by policy';
   END IF;
   SET v_payment=CAST(v_face AS UNSIGNED)*p_quantity;
   SET v_premium=(CAST(v_face AS UNSIGNED)*v_premium_bps DIV 10000)*p_quantity;
   IF v_payment > 1000000000 OR v_payment > v_max_transaction OR
      v_account_balance < v_payment OR v_balance > 2000000000-v_payment THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond purchase insufficient funds or cap';
   END IF;
   SELECT COALESCE(MAX(quantity),0) INTO v_existing_qty
     FROM government_bond_holdings WHERE issue_id=v_issue AND account_id=p_account_id;
   IF v_existing_qty+p_quantity > GREATEST(1,v_max_qty DIV 5) THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond concentration limit';
   END IF;
   UPDATE accounts SET balance=balance-v_payment,txn_serial=txn_serial+1,
        row_version=row_version+1 WHERE account_id=p_account_id;
   UPDATE treasury_accounts SET balance=balance+v_payment,
        bond_fund_reserved=bond_fund_reserved+v_premium,
        row_version=row_version+1 WHERE code='state';
   UPDATE government_bond_issues SET sold_quantity=sold_quantity+p_quantity,
        row_version=row_version+1 WHERE issue_id=v_issue;
   INSERT INTO government_bond_holdings(issue_id,account_id,quantity)
        VALUES(v_issue,p_account_id,p_quantity)
        ON DUPLICATE KEY UPDATE quantity=quantity+VALUES(quantity),
        row_version=row_version+1;
   INSERT INTO treasury_ledger
        (event_key,amount_delta,balance_after,source,actor_account_id,note)
        VALUES(p_event_key,v_payment,v_balance+v_payment,
               'bond_placement',p_account_id,p_issue_code);
   IF v_premium > 0 THEN
     INSERT INTO bond_fund_ledger
       (event_key,operation,reference_code,reserve_delta,commitment_delta,
        reserved_after,committed_after,treasury_row_version_after,actor_account_id)
       VALUES(p_event_key,'PREMIUM',p_issue_code,v_premium,0,
              v_reserved+v_premium,v_committed,v_treasury_version+1,p_account_id);
   END IF;
   INSERT INTO government_bond_orders
     (event_key,issue_id,account_id,quantity,amount_paid,fund_premium,
      account_balance_after,account_version_after,account_serial_after,
      treasury_balance_after,treasury_version_after)
     VALUES(p_event_key,v_issue,p_account_id,p_quantity,v_payment,v_premium,
            v_account_balance-v_payment,v_account_version+1,v_serial+1,
            v_balance+v_payment,v_treasury_version+1);
 END IF;
 -- For idempotent replay return authoritative current balances, not historical ones.
 SELECT balance,row_version,txn_serial INTO v_account_balance,v_account_version,v_serial
   FROM accounts WHERE account_id=p_account_id;
 SELECT balance,row_version INTO v_balance,v_treasury_version
   FROM treasury_accounts WHERE code='state';
 COMMIT;
 SELECT v_issue AS issue_id,v_account_balance AS account_balance,
        v_account_version AS account_row_version,v_serial AS account_txn_serial,
        v_balance AS treasury_balance,v_treasury_version AS treasury_row_version,
        v_replayed AS replayed;
END$$

-- Freeze all holders and assign a pro-rata portion of AVAILABLE state cash.
-- Principal comes before interest. No wallet credits until collect(), offline safe.
CREATE PROCEDURE gov_bond_maturity_settle(
 IN p_event_key VARCHAR(64), IN p_issue_code VARCHAR(64)
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_balance BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_fund BIGINT UNSIGNED;
 DECLARE v_pool BIGINT UNSIGNED;
 DECLARE v_minimum BIGINT UNSIGNED;
 DECLARE v_version INT UNSIGNED;
 DECLARE v_enabled TINYINT UNSIGNED;
 DECLARE v_expense_enabled TINYINT UNSIGNED;
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_status VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_sold INT UNSIGNED;
 DECLARE v_face INT UNSIGNED;
 DECLARE v_yield SMALLINT UNSIGNED;
 DECLARE v_maturity DATETIME;
 DECLARE v_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_principal BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_interest BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_free BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_principal_budget BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_interest_budget BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_awarded BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_count BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
    OR LEFT(BINARY p_event_key,5) <> BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_issue_code IS NULL OR CHAR_LENGTH(p_issue_code) NOT BETWEEN 1 AND 64
    OR p_issue_code NOT REGEXP '^[A-Za-z0-9_:/.-]+$' THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid maturity event';
 END IF;
 START TRANSACTION;
 SELECT balance,bond_fund_reserved,bond_payout_reserved,minimum_reserve,
        row_version,enabled,expense_enabled
   INTO v_balance,v_fund,v_pool,v_minimum,v_version,v_enabled,v_expense_enabled
   FROM treasury_accounts WHERE code='state' FOR UPDATE;
 SELECT issue_id,status,sold_quantity,face_value,yield_basis_points,
        maturity_at,maturity_event_key
   INTO v_issue,v_status,v_sold,v_face,v_yield,v_maturity,v_key
   FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE;
 IF v_balance IS NULL OR v_issue IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='maturity account or issue missing';
 END IF;
 IF v_key IS NOT NULL THEN
   IF BINARY v_key <> BINARY p_event_key THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='maturity already assessed';
   END IF;
   SET v_replayed=1;
 ELSE
   IF v_status NOT IN ('OFFERING','APPROVED','CLOSED')
      OR v_maturity > UTC_TIMESTAMP() OR v_version >= 2147483646 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='maturity not due';
   END IF;
   SELECT COALESCE(SUM(CAST(quantity AS UNSIGNED)),0) INTO v_count
     FROM government_bond_holdings WHERE issue_id=v_issue;
   IF v_count <> v_sold THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond holdings quantity mismatch';
   END IF;
   -- Use exact per-security fixed interest to prevent holder splitting rounding.
   SET v_principal = CAST(v_face AS UNSIGNED)*v_sold;
   SET v_interest = (CAST(v_face AS UNSIGNED)*v_yield DIV 10000)*v_sold;
   IF v_enabled=1 AND v_expense_enabled=1 AND
      v_balance >= v_minimum+v_fund+v_pool THEN
     SET v_free = v_balance-v_minimum-v_fund-v_pool;
   END IF;
   SET v_principal_budget=LEAST(v_free,v_principal);
   SET v_interest_budget=LEAST(GREATEST(0,v_free-v_principal_budget),v_interest);
   INSERT INTO government_bond_claims(issue_id,account_id,principal_due,interest_due)
     SELECT v_issue,account_id,CAST(v_face AS UNSIGNED)*quantity,
            (CAST(v_face AS UNSIGNED)*v_yield DIV 10000)*quantity
     FROM government_bond_holdings WHERE issue_id=v_issue;
   IF v_principal>0 AND v_principal_budget>0 THEN
     UPDATE government_bond_claims SET principal_budget_awarded=
       (CAST(principal_due AS DECIMAL(30,0))*v_principal_budget DIV v_principal)
       WHERE issue_id=v_issue;
   END IF;
   IF v_interest>0 AND v_interest_budget>0 THEN
     UPDATE government_bond_claims SET interest_budget_awarded=
       (CAST(interest_due AS DECIMAL(30,0))*v_interest_budget DIV v_interest)
       WHERE issue_id=v_issue;
   END IF;
   SELECT COALESCE(SUM(principal_budget_awarded+interest_budget_awarded),0)
     INTO v_awarded FROM government_bond_claims WHERE issue_id=v_issue;
   IF v_awarded>v_free OR v_pool+v_awarded>2000000000 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid pro rata maturity allocation';
   END IF;
   UPDATE treasury_accounts SET bond_payout_reserved=bond_payout_reserved+v_awarded,
     row_version=row_version+1 WHERE code='state';
   SET v_pool=v_pool+v_awarded;
   SET v_version=v_version+1;
   INSERT INTO bond_payout_ledger
     (event_key,operation,issue_id,reserved_delta,reserved_after,treasury_row_version_after)
     VALUES(p_event_key,'MATURITY',v_issue,v_awarded,v_pool,v_version);
   UPDATE government_bond_issues SET status='CLOSED',
     debt_status=IF(v_principal+v_interest=v_awarded,'SETTLED','DUE'),
     maturity_event_key=p_event_key,row_version=row_version+1 WHERE issue_id=v_issue;
 END IF;
 COMMIT;
 SELECT v_issue AS issue_id,v_awarded AS newly_awarded,
        v_pool AS payout_reserved,v_replayed AS replayed;
END$$

-- Twenty-four-hour grace (snapshot stored at APPROVAL); fund transfers money
-- to a dedicated escrow, so it cannot be spent again before an owner collects.
CREATE PROCEDURE gov_bond_insurance_assess(
 IN p_event_key VARCHAR(64), IN p_issue_code VARCHAR(64)
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_balance BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_fund BIGINT UNSIGNED;
 DECLARE v_pool BIGINT UNSIGNED;
 DECLARE v_committed BIGINT UNSIGNED;
 DECLARE v_version INT UNSIGNED;
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_maturity DATETIME;
 DECLARE v_grace INT UNSIGNED;
 DECLARE v_coverage SMALLINT UNSIGNED;
 DECLARE v_maturity_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_insurance_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_exposure BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_released BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_amount BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_remaining_due BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
    OR LEFT(BINARY p_event_key,5) <> BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_issue_code IS NULL OR CHAR_LENGTH(p_issue_code) NOT BETWEEN 1 AND 64
    OR p_issue_code NOT REGEXP '^[A-Za-z0-9_:/.-]+$' THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid insurance event';
 END IF;
 START TRANSACTION;
 SELECT balance,bond_fund_reserved,bond_payout_reserved,
        bond_fund_committed,row_version
   INTO v_balance,v_fund,v_pool,v_committed,v_version
   FROM treasury_accounts WHERE code='state' FOR UPDATE;
 SELECT issue_id,maturity_at,grace_seconds,coverage_basis_points,
        maturity_event_key,insurance_event_key
   INTO v_issue,v_maturity,v_grace,v_coverage,v_maturity_key,v_insurance_key
   FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE;
 IF v_balance IS NULL OR v_issue IS NULL OR v_maturity_key IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='maturity must settle before insurance';
 END IF;
 IF v_insurance_key IS NOT NULL THEN
   IF BINARY v_insurance_key <> BINARY p_event_key THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='insurance already assessed';
   END IF;
   SET v_replayed=1;
 ELSE
   IF UTC_TIMESTAMP() < DATE_ADD(v_maturity,INTERVAL v_grace SECOND)
      OR v_version >= 2147483646 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='insurance grace not expired';
   END IF;
   SELECT max_exposure,released_exposure INTO v_exposure,v_released
      FROM bond_fund_guarantees WHERE issue_code=p_issue_code FOR UPDATE;
   IF v_exposure IS NULL OR v_released <> 0 OR v_committed < v_exposure THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='guarantee missing or already released';
   END IF;
   UPDATE government_bond_claims SET insurance_awarded=
     ((CAST(principal_due-principal_budget_awarded AS DECIMAL(30,0))
        *v_coverage) DIV 10000) WHERE issue_id=v_issue;
   SELECT COALESCE(SUM(insurance_awarded),0),
          COALESCE(SUM(principal_due+interest_due-
            principal_budget_awarded-interest_budget_awarded-insurance_awarded),0)
     INTO v_amount,v_remaining_due
     FROM government_bond_claims WHERE issue_id=v_issue;
   IF v_amount>v_exposure OR v_amount>v_fund OR
      v_pool+v_amount>2000000000 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='insurance coverage overdraw';
   END IF;
   UPDATE treasury_accounts SET
      bond_fund_reserved=bond_fund_reserved-v_amount,
      bond_fund_committed=bond_fund_committed-v_exposure,
      bond_payout_reserved=bond_payout_reserved+v_amount,
      row_version=row_version+1 WHERE code='state';
   SET v_fund=v_fund-v_amount;
   SET v_committed=v_committed-v_exposure;
   SET v_pool=v_pool+v_amount;
   SET v_version=v_version+1;
   UPDATE bond_fund_guarantees
      SET released_exposure=max_exposure WHERE issue_code=p_issue_code;
   INSERT INTO bond_fund_ledger
      (event_key,operation,reference_code,reserve_delta,commitment_delta,
       reserved_after,committed_after,treasury_row_version_after)
      VALUES(p_event_key,'INSURANCE_ALLOCATE',p_issue_code,-v_amount,-v_exposure,
             v_fund,v_committed,v_version);
   INSERT INTO bond_payout_ledger
      (event_key,operation,issue_id,reserved_delta,reserved_after,treasury_row_version_after)
      VALUES(p_event_key,'INSURANCE',v_issue,v_amount,v_pool,v_version);
   UPDATE government_bond_issues SET
     insurance_event_key=p_event_key,
     debt_status=IF(v_remaining_due=0,'SETTLED','DEFAULTED'),
     row_version=row_version+1 WHERE issue_id=v_issue;
 END IF;
 COMMIT;
 SELECT v_issue AS issue_id,v_amount AS newly_insured,
        v_pool AS payout_reserved,v_replayed AS replayed;
END$$

-- Only this function moves held money from payout escrow into player account;
-- it checks account row_version so DB and Pawn session cash stay consistent.
CREATE PROCEDURE gov_bond_claim_collect(
 IN p_event_key VARCHAR(64), IN p_claim_id BIGINT UNSIGNED,
 IN p_account_id BIGINT UNSIGNED, IN p_amount INT,
 IN p_expected_version INT
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_balance BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_pool BIGINT UNSIGNED;
 DECLARE v_fund BIGINT UNSIGNED;
 DECLARE v_treasury_version INT UNSIGNED;
 DECLARE v_account_balance BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_account_version INT UNSIGNED;
 DECLARE v_account_serial BIGINT UNSIGNED;
 DECLARE v_claim BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_issue BIGINT UNSIGNED;
 DECLARE v_owner BIGINT UNSIGNED;
 DECLARE v_principal BIGINT UNSIGNED;
 DECLARE v_interest BIGINT UNSIGNED;
 DECLARE v_insurance BIGINT UNSIGNED;
 DECLARE v_collected BIGINT UNSIGNED;
 DECLARE v_existing BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_existing_claim BIGINT UNSIGNED;
 DECLARE v_existing_owner BIGINT UNSIGNED;
 DECLARE v_existing_amount BIGINT UNSIGNED;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;

 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
    OR LEFT(BINARY p_event_key,5) <> BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_claim_id IS NULL OR p_claim_id=0 OR p_account_id IS NULL OR p_account_id=0
    OR p_amount IS NULL OR p_amount<=0 OR p_amount>1000000000
    OR p_expected_version IS NULL OR p_expected_version<0
    OR p_expected_version>=2147483646 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid bond claim collection';
 END IF;
 START TRANSACTION;
 SELECT balance,bond_payout_reserved,bond_fund_reserved,row_version
    INTO v_balance,v_pool,v_fund,v_treasury_version FROM treasury_accounts
    WHERE code='state' FOR UPDATE;
 SELECT balance,row_version,txn_serial
    INTO v_account_balance,v_account_version,v_account_serial
    FROM accounts WHERE account_id=p_account_id FOR UPDATE;
 SELECT claim_id,issue_id,account_id,principal_budget_awarded,
        interest_budget_awarded,insurance_awarded,amount_collected
    INTO v_claim,v_issue,v_owner,v_principal,v_interest,v_insurance,v_collected
    FROM government_bond_claims WHERE claim_id=p_claim_id FOR UPDATE;
 IF v_balance IS NULL OR v_account_balance IS NULL OR v_claim IS NULL
    OR v_owner<>p_account_id THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='claim missing or not owned';
 END IF;
 SELECT receipt_id,claim_id,account_id,amount
    INTO v_existing,v_existing_claim,v_existing_owner,v_existing_amount
    FROM government_bond_claim_receipts WHERE event_key=p_event_key LIMIT 1;
 IF v_existing IS NOT NULL THEN
   IF v_existing_claim<>p_claim_id OR v_existing_owner<>p_account_id
      OR v_existing_amount<>p_amount THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='claim receipt event collision';
   END IF;
   SET v_replayed=1;
 ELSE
   IF v_treasury_version >= 2147483646 OR
      v_account_version <> p_expected_version OR
      v_account_version >= 2147483646 OR v_account_serial >= 1000000000 OR
      v_principal+v_interest+v_insurance-v_collected<p_amount OR
      v_pool<p_amount OR v_balance<p_amount OR
      v_account_balance>1000000000-p_amount THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='claim collection exceeds available funds';
   END IF;
   UPDATE accounts SET balance=balance+p_amount,row_version=row_version+1,
      txn_serial=txn_serial+1 WHERE account_id=p_account_id;
   UPDATE treasury_accounts SET balance=balance-p_amount,
      bond_payout_reserved=bond_payout_reserved-p_amount,
      row_version=row_version+1 WHERE code='state';
   UPDATE government_bond_claims SET amount_collected=amount_collected+p_amount
      WHERE claim_id=p_claim_id;
   INSERT INTO treasury_ledger
      (event_key,amount_delta,balance_after,source,actor_account_id,note)
      VALUES(p_event_key,-p_amount,v_balance-p_amount,'bond_collect',
             p_account_id,CAST(v_issue AS CHAR));
   INSERT INTO bond_payout_ledger
      (event_key,operation,issue_id,reserved_delta,reserved_after,treasury_row_version_after)
      VALUES(p_event_key,'COLLECT',v_issue,-p_amount,v_pool-p_amount,
             v_treasury_version+1);
   INSERT INTO government_bond_claim_receipts
      (event_key,claim_id,account_id,amount,account_balance_after,
       account_version_after,account_serial_after,treasury_version_after)
      VALUES(p_event_key,p_claim_id,p_account_id,p_amount,
             v_account_balance+p_amount,v_account_version+1,v_account_serial+1,
             v_treasury_version+1);
 END IF;
 SELECT balance,row_version,txn_serial
   INTO v_account_balance,v_account_version,v_account_serial
   FROM accounts WHERE account_id=p_account_id;
 SELECT balance,row_version INTO v_balance,v_treasury_version
   FROM treasury_accounts WHERE code='state';
 COMMIT;
 SELECT v_claim AS claim_id,v_account_balance AS account_balance,
        v_account_version AS account_row_version,
        v_account_serial AS account_txn_serial,
        v_balance AS treasury_balance,v_treasury_version AS treasury_row_version,
        v_replayed AS replayed;
END$$
DELIMITER ;

INSERT INTO schema_migrations(version) VALUES (21);
