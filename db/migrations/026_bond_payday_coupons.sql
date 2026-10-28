-- GOV-07 / migration 026: hourly/offline coupons on a conditional account.
-- No new money is created at accrual: coupon wallet is an unfunded receivable.
-- A manual collection is a real, auditable, protected treasury transfer.
-- Apply only AFTER 025; never install on production before GOV-07 UAT.

CREATE TABLE government_bond_coupon_positions (
  order_id BIGINT UNSIGNED NOT NULL PRIMARY KEY,
  issue_id BIGINT UNSIGNED NOT NULL,
  account_id BIGINT UNSIGNED NOT NULL,
  accrued BIGINT UNSIGNED NOT NULL DEFAULT 0,
  last_hour_key INT UNSIGNED NOT NULL DEFAULT 0,
  CONSTRAINT fk_coupon_position_order FOREIGN KEY (order_id)
    REFERENCES government_bond_orders(order_id) ON DELETE RESTRICT,
  KEY idx_coupon_position_issue (issue_id,account_id),
  CONSTRAINT chk_coupon_position_amount CHECK (accrued<=1000000000)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE government_bond_coupon_wallets (
  issue_id BIGINT UNSIGNED NOT NULL,
  account_id BIGINT UNSIGNED NOT NULL,
  accrued BIGINT UNSIGNED NOT NULL DEFAULT 0,
  collected BIGINT UNSIGNED NOT NULL DEFAULT 0,
  converted BIGINT UNSIGNED NOT NULL DEFAULT 0,
  last_hour_key INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (issue_id,account_id),
  KEY idx_coupon_wallet_account (account_id,issue_id),
  CONSTRAINT fk_coupon_wallet_issue FOREIGN KEY (issue_id)
    REFERENCES government_bond_issues(issue_id) ON DELETE RESTRICT,
  CONSTRAINT fk_coupon_wallet_owner FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_coupon_wallet CHECK (
    accrued<=1000000000 AND collected+converted<=accrued)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE government_bond_coupon_events (
  issue_id BIGINT UNSIGNED NOT NULL,
  hour_key INT UNSIGNED NOT NULL,
  newly_accrued BIGINT UNSIGNED NOT NULL,
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  PRIMARY KEY (issue_id,hour_key),
  CONSTRAINT fk_coupon_event_issue FOREIGN KEY (issue_id)
    REFERENCES government_bond_issues(issue_id) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE government_bond_coupon_receipts (
  receipt_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  issue_id BIGINT UNSIGNED NOT NULL,
  account_id BIGINT UNSIGNED NOT NULL,
  amount INT UNSIGNED NOT NULL,
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  UNIQUE KEY uq_coupon_receipt_event(event_key),
  KEY idx_coupon_receipt_owner (account_id,issue_id),
  CONSTRAINT fk_coupon_receipt_wallet FOREIGN KEY (issue_id,account_id)
    REFERENCES government_bond_coupon_wallets(issue_id,account_id)
    ON DELETE RESTRICT,
  CONSTRAINT chk_coupon_receipt_amount CHECK(amount BETWEEN 1 AND 1000000000)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DELIMITER $$
CREATE TRIGGER gov_coupon_events_upd BEFORE UPDATE ON government_bond_coupon_events
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='coupon event audit immutable';
END$$
CREATE TRIGGER gov_coupon_events_del BEFORE DELETE ON government_bond_coupon_events
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='coupon event audit immutable';
END$$
CREATE TRIGGER gov_coupon_receipts_upd BEFORE UPDATE ON government_bond_coupon_receipts
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='coupon receipt audit immutable';
END$$
CREATE TRIGGER gov_coupon_receipts_del BEFORE DELETE ON government_bond_coupon_receipts
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='coupon receipt audit immutable';
END$$

CREATE PROCEDURE gov_bond_coupon_accrue(
 IN p_issue_id BIGINT UNSIGNED,IN p_hour_key INT UNSIGNED
)
SQL SECURITY DEFINER
BEGIN
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_status VARCHAR(16);
 DECLARE v_maturity DATETIME;
 DECLARE v_face BIGINT UNSIGNED;
 DECLARE v_yield INT UNSIGNED;
 DECLARE v_before BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_after BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE v_exists INT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 IF p_issue_id IS NULL OR p_issue_id=0 OR p_hour_key IS NULL OR
    p_hour_key<400000 OR p_hour_key>4100000 OR
    p_hour_key>UNIX_TIMESTAMP(UTC_TIMESTAMP()) DIV 3600 THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid coupon payday';
 END IF;
 START TRANSACTION;
 SELECT issue_id,status,maturity_at,face_value,yield_basis_points
   INTO v_issue,v_status,v_maturity,v_face,v_yield
   FROM government_bond_issues WHERE issue_id=p_issue_id FOR UPDATE;
 IF v_issue IS NULL OR v_status<>'OFFERING'
    OR DATE_ADD('1970-01-01 00:00:00',INTERVAL p_hour_key HOUR)>v_maturity THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='coupon issue not eligible';
 END IF;
 SELECT COUNT(*) INTO v_exists FROM government_bond_coupon_events
   WHERE issue_id=p_issue_id AND hour_key=p_hour_key;
 IF v_exists>0 THEN
   SET v_replayed=1;
 ELSE
   SELECT COALESCE(SUM(accrued),0) INTO v_before
     FROM government_bond_coupon_wallets WHERE issue_id=v_issue;
   INSERT IGNORE INTO government_bond_coupon_positions(order_id,issue_id,account_id)
     SELECT o.order_id,o.issue_id,o.account_id FROM government_bond_orders o
       WHERE o.issue_id=v_issue
         AND o.created_at < DATE_ADD('1970-01-01 00:00:00',INTERVAL p_hour_key HOUR);
   -- Each purchase is its own tranche. Integer cumulative differences ensure
   -- the sum of all coupons per tranche never exceeds the agreed total yield.
   UPDATE government_bond_coupon_positions p
     JOIN government_bond_orders o ON o.order_id=p.order_id
     SET p.accrued=
        (CAST((v_face*v_yield DIV 10000)*o.quantity AS DECIMAL(30,0))*
          LEAST(p_hour_key-TIMESTAMPDIFF(HOUR,'1970-01-01 00:00:00',o.created_at),
            TIMESTAMPDIFF(HOUR,'1970-01-01 00:00:00',v_maturity)
            -TIMESTAMPDIFF(HOUR,'1970-01-01 00:00:00',o.created_at)))
          DIV (TIMESTAMPDIFF(HOUR,'1970-01-01 00:00:00',v_maturity)
            -TIMESTAMPDIFF(HOUR,'1970-01-01 00:00:00',o.created_at)),
        p.last_hour_key=p_hour_key
     WHERE p.issue_id=v_issue
       AND p_hour_key>TIMESTAMPDIFF(HOUR,'1970-01-01 00:00:00',o.created_at)
       AND TIMESTAMPDIFF(HOUR,'1970-01-01 00:00:00',v_maturity)>
           TIMESTAMPDIFF(HOUR,'1970-01-01 00:00:00',o.created_at)
       AND p.last_hour_key<p_hour_key;
   INSERT INTO government_bond_coupon_wallets
     (issue_id,account_id,accrued,last_hour_key)
     SELECT p.issue_id,p.account_id,SUM(p.accrued),p_hour_key
     FROM government_bond_coupon_positions p WHERE p.issue_id=v_issue
     GROUP BY p.issue_id,p.account_id
     ON DUPLICATE KEY UPDATE
       accrued=VALUES(accrued),last_hour_key=VALUES(last_hour_key);
   SELECT COALESCE(SUM(accrued),0) INTO v_after
     FROM government_bond_coupon_wallets WHERE issue_id=v_issue;
   IF v_after<v_before OR v_after>1000000000 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='coupon accrual invariant violated';
   END IF;
   INSERT INTO government_bond_coupon_events(issue_id,hour_key,newly_accrued)
     VALUES(v_issue,p_hour_key,v_after-v_before);
 END IF;
 COMMIT;
 SELECT v_issue AS issue_id,v_after-v_before AS newly_accrued,
   v_replayed AS replayed;
END$$

CREATE PROCEDURE gov_bond_coupon_collect(
 IN p_event_key VARCHAR(64), IN p_issue_id BIGINT UNSIGNED,
 IN p_account_id BIGINT UNSIGNED, IN p_amount INT,
 IN p_expected_version INT
)
SQL SECURITY DEFINER
BEGIN
 DECLARE v_treasury BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_pool BIGINT UNSIGNED;
 DECLARE v_fund BIGINT UNSIGNED;
 DECLARE v_min BIGINT UNSIGNED;
 DECLARE v_treasury_version INT UNSIGNED;
 DECLARE v_max_tx BIGINT UNSIGNED;
 DECLARE v_enabled TINYINT UNSIGNED;
 DECLARE v_out TINYINT UNSIGNED;
 DECLARE v_account BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_account_version INT UNSIGNED;
 DECLARE v_serial BIGINT UNSIGNED;
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_maturity_key VARCHAR(64);
 DECLARE v_maturity DATETIME;
 DECLARE v_accrued BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_collected BIGINT UNSIGNED;
 DECLARE v_converted BIGINT UNSIGNED;
 DECLARE v_old_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_old_account BIGINT UNSIGNED;
 DECLARE v_old_amount INT UNSIGNED;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
    OR LEFT(BINARY p_event_key,5)<>BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_issue_id IS NULL OR p_issue_id=0
    OR p_account_id IS NULL OR p_account_id=0
    OR p_amount IS NULL OR p_amount<1 OR p_amount>1000000000
    OR p_expected_version IS NULL OR p_expected_version<0
    OR p_expected_version>=2147483646 THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid coupon collection';
 END IF;
 START TRANSACTION;
 SELECT balance,bond_fund_reserved,bond_payout_reserved,minimum_reserve,
       row_version,max_transaction,enabled,expense_enabled
   INTO v_treasury,v_fund,v_pool,v_min,v_treasury_version,v_max_tx,
       v_enabled,v_out
   FROM treasury_accounts WHERE code='state' FOR UPDATE;
 SELECT balance,row_version,txn_serial INTO v_account,v_account_version,v_serial
   FROM accounts WHERE account_id=p_account_id FOR UPDATE;
 SELECT issue_id,maturity_event_key,maturity_at
   INTO v_issue,v_maturity_key,v_maturity
   FROM government_bond_issues WHERE issue_id=p_issue_id FOR UPDATE;
 SELECT accrued,collected,converted INTO v_accrued,v_collected,v_converted
   FROM government_bond_coupon_wallets
   WHERE issue_id=p_issue_id AND account_id=p_account_id FOR UPDATE;
 IF v_treasury IS NULL OR v_account IS NULL OR v_issue IS NULL
    OR v_accrued IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='coupon wallet not found';
 END IF;
 SELECT issue_id,account_id,amount
   INTO v_old_issue,v_old_account,v_old_amount
   FROM government_bond_coupon_receipts WHERE event_key=p_event_key LIMIT 1;
 IF v_old_issue IS NOT NULL THEN
   IF v_old_issue<>v_issue OR v_old_account<>p_account_id
      OR v_old_amount<>p_amount THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='coupon replay collision';
   END IF;
   SET v_replayed=1;
 ELSE
   IF v_maturity_key IS NOT NULL OR UTC_TIMESTAMP()>=v_maturity OR
      v_enabled<>1 OR v_out<>1 OR p_amount>v_max_tx OR
      p_amount>v_accrued-v_collected-v_converted OR
      v_treasury<v_min+v_fund+v_pool+p_amount OR
      v_account>1000000000-p_amount OR
      v_account_version<>p_expected_version OR
      v_account_version>=2147483646 OR v_serial>=1000000000 OR
      v_treasury_version>=2147483646 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='coupon collection cannot be funded';
   END IF;
   UPDATE government_bond_coupon_wallets SET collected=collected+p_amount
     WHERE issue_id=v_issue AND account_id=p_account_id;
   UPDATE accounts SET balance=balance+p_amount,row_version=row_version+1,
     txn_serial=txn_serial+1 WHERE account_id=p_account_id;
   UPDATE treasury_accounts SET balance=balance-p_amount,row_version=row_version+1
     WHERE code='state';
   INSERT INTO treasury_ledger
      (event_key,amount_delta,balance_after,source,actor_account_id,note)
     VALUES(p_event_key,-p_amount,v_treasury-p_amount,'bond_coupon',
       p_account_id,CAST(v_issue AS CHAR));
   INSERT INTO government_bond_coupon_receipts(event_key,issue_id,account_id,amount)
     VALUES(p_event_key,v_issue,p_account_id,p_amount);
 END IF;
 SELECT balance,row_version,txn_serial INTO v_account,v_account_version,v_serial
   FROM accounts WHERE account_id=p_account_id;
 SELECT balance,row_version INTO v_treasury,v_treasury_version
   FROM treasury_accounts WHERE code='state';
 COMMIT;
 SELECT v_issue AS issue_id,v_account AS account_balance,
    v_account_version AS account_row_version,
    v_serial AS account_txn_serial,v_treasury AS treasury_balance,
    v_treasury_version AS treasury_row_version,v_replayed AS replayed;
END$$

-- Rebuild maturity assessment to subtract already-collected coupons once.
DROP PROCEDURE IF EXISTS gov_bond_maturity_settle$$
CREATE PROCEDURE gov_bond_maturity_settle(
 IN p_event_key VARCHAR(64), IN p_issue_code VARCHAR(64)
)
SQL SECURITY DEFINER
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
 DECLARE v_coupon_paid BIGINT UNSIGNED DEFAULT 0;
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
   SELECT COALESCE(SUM(collected),0) INTO v_coupon_paid
     FROM government_bond_coupon_wallets WHERE issue_id=v_issue;
   SET v_interest = (CAST(v_face AS UNSIGNED)*v_yield DIV 10000)*v_sold;
   IF v_coupon_paid>v_interest THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='coupon payments exceed contract yield';
   END IF;
   SET v_interest=v_interest-v_coupon_paid;
   IF v_enabled=1 AND v_expense_enabled=1 AND
      v_balance >= v_minimum+v_fund+v_pool THEN
     SET v_free = v_balance-v_minimum-v_fund-v_pool;
   END IF;
   SET v_principal_budget=LEAST(v_free,v_principal);
   SET v_interest_budget=LEAST(GREATEST(0,v_free-v_principal_budget),v_interest);
   -- Uncollected virtual coupons roll into the ordinary maturity claim.
   -- The claim interest is total contractual yield MINUS coupons already paid.
   UPDATE government_bond_coupon_wallets
     SET converted=accrued-collected WHERE issue_id=v_issue;
   INSERT INTO government_bond_claims(issue_id,account_id,principal_due,interest_due)
     SELECT v_issue,h.account_id,CAST(v_face AS UNSIGNED)*h.quantity,
       (CAST(v_face AS UNSIGNED)*v_yield DIV 10000)*h.quantity
         - COALESCE(w.collected,0)
     FROM government_bond_holdings h
     LEFT JOIN government_bond_coupon_wallets w
       ON w.issue_id=h.issue_id AND w.account_id=h.account_id
     WHERE h.issue_id=v_issue;
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

DELIMITER ;
INSERT INTO schema_migrations(version) VALUES (26);
