-- GOV-09: fixed 98% state buyback, no exchange or other-player trading.
-- Apply once after 029 with server stopped and DBA backup. Schema and routines
-- must migrate together; do not deploy Pawn until SQL 030 passes MariaDB UAT.
ALTER TABLE government_bond_issues
 ADD COLUMN bought_back_quantity INT UNSIGNED NOT NULL DEFAULT 0,
 ADD CONSTRAINT chk_gov09_issue_boughtback CHECK (bought_back_quantity<=sold_quantity);
ALTER TABLE government_bond_coupon_positions
 ADD COLUMN frozen_accrued BIGINT UNSIGNED NOT NULL DEFAULT 0,
 ADD CONSTRAINT chk_gov09_coupon_frozen CHECK(frozen_accrued<=accrued);
-- A holder selling ALL securities may still have earned but uncollected coupons.
ALTER TABLE government_bond_claims
 DROP CONSTRAINT chk_bond_claim_amounts,
 ADD CONSTRAINT chk_bond_claim_amounts CHECK
   ((principal_due>0 OR interest_due>0)
    AND principal_budget_awarded<=principal_due
    AND interest_budget_awarded<=interest_due
    AND insurance_awarded<=principal_due-principal_budget_awarded
    AND amount_collected<=principal_budget_awarded+interest_budget_awarded+insurance_awarded);

CREATE TABLE government_bond_buybacks (
 buyback_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
 event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 issue_id BIGINT UNSIGNED NOT NULL,
 account_id BIGINT UNSIGNED NOT NULL,
 quantity INT UNSIGNED NOT NULL,
 unit_price BIGINT UNSIGNED NOT NULL,
 amount_paid BIGINT UNSIGNED NOT NULL,
 released_exposure BIGINT UNSIGNED NOT NULL,
 created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
 UNIQUE KEY uq_buyback_event(event_key),
 KEY idx_buyback_issue(issue_id,buyback_id),
 KEY idx_buyback_account(account_id,buyback_id),
 CONSTRAINT fk_buyback_issue FOREIGN KEY(issue_id) REFERENCES government_bond_issues(issue_id) ON DELETE RESTRICT,
 CONSTRAINT fk_buyback_account FOREIGN KEY(account_id) REFERENCES accounts(account_id) ON DELETE RESTRICT,
 CONSTRAINT chk_buyback_amount CHECK(quantity>0 AND unit_price>0 AND amount_paid BETWEEN 1 AND 1000000000)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE government_bond_buyback_allocations (
 buyback_id BIGINT UNSIGNED NOT NULL,
 order_id BIGINT UNSIGNED NOT NULL,
 quantity INT UNSIGNED NOT NULL,
 frozen_coupon_amount BIGINT UNSIGNED NOT NULL,
 PRIMARY KEY(buyback_id,order_id),
 KEY idx_buyback_allocation_order(order_id),
 CONSTRAINT fk_buyback_allocation_buyback FOREIGN KEY(buyback_id)
    REFERENCES government_bond_buybacks(buyback_id) ON DELETE RESTRICT,
 CONSTRAINT fk_buyback_allocation_order FOREIGN KEY(order_id)
    REFERENCES government_bond_orders(order_id) ON DELETE RESTRICT,
 CONSTRAINT chk_buyback_alloc_quantity CHECK(quantity>0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
DELIMITER $$
CREATE TRIGGER gov09_buybacks_upd BEFORE UPDATE ON government_bond_buybacks
FOR EACH ROW BEGIN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='buyback audit immutable'; END$$
CREATE TRIGGER gov09_buybacks_del BEFORE DELETE ON government_bond_buybacks
FOR EACH ROW BEGIN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='buyback audit immutable'; END$$
CREATE TRIGGER gov09_buyback_alloc_upd BEFORE UPDATE ON government_bond_buyback_allocations
FOR EACH ROW BEGIN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='buyback allocation audit immutable'; END$$
CREATE TRIGGER gov09_buyback_alloc_del BEFORE DELETE ON government_bond_buyback_allocations
FOR EACH ROW BEGIN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='buyback allocation audit immutable'; END$$

-- GOV-09 fixed-price (98% face) state buyback. No player-to-player market.
CREATE PROCEDURE gov_bond_buyback(
 IN p_event_key VARCHAR(64),IN p_issue_id BIGINT UNSIGNED,
 IN p_account_id BIGINT UNSIGNED,IN p_quantity INT,IN p_expected_version INT
)
SQL SECURITY DEFINER
BEGIN
 DECLARE v_balance BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_fund BIGINT UNSIGNED;
 DECLARE v_pool BIGINT UNSIGNED;
 DECLARE v_min BIGINT UNSIGNED;
 DECLARE v_committed BIGINT UNSIGNED;
 DECLARE v_max_tx BIGINT UNSIGNED;
 DECLARE v_treasury_version INT UNSIGNED;
 DECLARE v_enabled TINYINT UNSIGNED;
 DECLARE v_expense TINYINT UNSIGNED;
 DECLARE v_account BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_account_version INT UNSIGNED;
 DECLARE v_serial BIGINT UNSIGNED;
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_code VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_status VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_maturity DATETIME;
 DECLARE v_maturity_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_face INT UNSIGNED;
 DECLARE v_yield SMALLINT UNSIGNED;
 DECLARE v_max_qty INT UNSIGNED;
 DECLARE v_boughtback INT UNSIGNED;
 DECLARE v_held INT UNSIGNED DEFAULT NULL;
 DECLARE v_locked INT UNSIGNED;
 DECLARE v_max_exposure BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_released BIGINT UNSIGNED;
 DECLARE v_release BIGINT UNSIGNED;
 DECLARE v_release_target BIGINT UNSIGNED;
 DECLARE v_unit BIGINT UNSIGNED;
 DECLARE v_payment BIGINT UNSIGNED;
 DECLARE v_existing BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_existing_issue BIGINT UNSIGNED;
 DECLARE v_existing_owner BIGINT UNSIGNED;
 DECLARE v_existing_quantity INT UNSIGNED;
 DECLARE v_remaining INT UNSIGNED;
 DECLARE v_order BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_order_qty INT UNSIGNED;
 DECLARE v_order_redeemed INT UNSIGNED;
 DECLARE v_slice INT UNSIGNED;
 DECLARE v_purchase DATETIME;
 DECLARE v_p_hour BIGINT;
 DECLARE v_m_hour BIGINT;
 DECLARE v_last_hour BIGINT;
 DECLARE v_accrued BIGINT UNSIGNED;
 DECLARE v_frozen BIGINT UNSIGNED;
 DECLARE v_active_coupon BIGINT UNSIGNED;
 DECLARE v_new_frozen BIGINT UNSIGNED;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
    OR LEFT(BINARY p_event_key,5)<>BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_issue_id IS NULL OR p_issue_id=0 OR p_account_id IS NULL OR p_account_id=0
    OR p_quantity IS NULL OR p_quantity<1 OR p_quantity>1000000
    OR p_expected_version IS NULL OR p_expected_version<0 OR p_expected_version>=2147483646 THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid bond buyback request';
 END IF;
 START TRANSACTION;
 -- Financial lock order: treasury -> player -> issue -> holdings -> orders.
 SELECT balance,bond_fund_reserved,bond_payout_reserved,minimum_reserve,
        bond_fund_committed,max_transaction,row_version,enabled,expense_enabled
   INTO v_balance,v_fund,v_pool,v_min,v_committed,v_max_tx,
        v_treasury_version,v_enabled,v_expense
   FROM treasury_accounts WHERE code='state' FOR UPDATE;
 SELECT balance,row_version,txn_serial
   INTO v_account,v_account_version,v_serial
   FROM accounts WHERE account_id=p_account_id FOR UPDATE;
 SELECT issue_id,issue_code,status,maturity_at,maturity_event_key,face_value,
        yield_basis_points,max_quantity,bought_back_quantity
   INTO v_issue,v_code,v_status,v_maturity,v_maturity_key,v_face,
        v_yield,v_max_qty,v_boughtback
   FROM government_bond_issues WHERE issue_id=p_issue_id FOR UPDATE;
 IF v_balance IS NULL OR v_account IS NULL OR v_issue IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='buyback account or issue missing';
 END IF;
 SELECT buyback_id,issue_id,account_id,quantity
   INTO v_existing,v_existing_issue,v_existing_owner,v_existing_quantity
   FROM government_bond_buybacks WHERE event_key=p_event_key LIMIT 1;
 IF v_existing IS NOT NULL THEN
   IF v_existing_issue<>v_issue OR v_existing_owner<>p_account_id
      OR v_existing_quantity<>p_quantity THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='buyback event collision';
   END IF;
   SET v_replayed=1;
 ELSE
   IF v_status NOT IN ('OFFERING','CLOSED','APPROVED')
      OR v_maturity_key IS NOT NULL OR UTC_TIMESTAMP()>=v_maturity
      OR v_enabled<>1 OR v_expense<>1
      OR v_account_version<>p_expected_version
      OR v_account_version>=2147483646 OR v_serial>=1000000000
      OR v_treasury_version>=2147483646 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='buyback unavailable by terms';
   END IF;
   SELECT quantity,locked_quantity INTO v_held,v_locked
     FROM government_bond_holdings
     WHERE issue_id=v_issue AND account_id=p_account_id FOR UPDATE;
   IF v_held IS NULL OR p_quantity>v_held-v_locked THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='insufficient owned bonds';
   END IF;
   SET v_unit=CAST(v_face AS UNSIGNED)*98 DIV 100;
   SET v_payment=v_unit*p_quantity;
   IF v_unit=0 OR v_payment=0 OR v_payment>1000000000
      OR v_payment>v_max_tx OR v_balance<v_min+v_fund+v_pool+v_payment
      OR v_account>1000000000-v_payment THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='treasury cannot fund buyback';
   END IF;
   SELECT max_exposure,released_exposure INTO v_max_exposure,v_released
     FROM bond_fund_guarantees WHERE issue_code=v_code FOR UPDATE;
   IF v_max_exposure IS NULL OR v_released>v_max_exposure
      OR v_boughtback+p_quantity>v_max_qty THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='buyback insurance state invalid';
   END IF;
   SET v_release_target=(CAST(v_max_exposure AS DECIMAL(30,0))
                          *(v_boughtback+p_quantity)) DIV v_max_qty;
   IF v_released<>((CAST(v_max_exposure AS DECIMAL(30,0))*v_boughtback) DIV v_max_qty)
      OR v_release_target<v_released OR
      v_committed<v_release_target-v_released THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='buyback guarantee mismatch';
   END IF;
   SET v_release=v_release_target-v_released;
   INSERT INTO government_bond_buybacks
     (event_key,issue_id,account_id,quantity,unit_price,amount_paid,released_exposure)
     VALUES(p_event_key,v_issue,p_account_id,p_quantity,v_unit,v_payment,v_release);
   SET v_existing=LAST_INSERT_ID();
   SET v_remaining=p_quantity;
   WHILE v_remaining>0 DO
     SET v_order=NULL;
     SELECT o.order_id,o.quantity,o.created_at
       INTO v_order,v_order_qty,v_purchase
       FROM government_bond_orders o
       WHERE o.issue_id=v_issue AND o.account_id=p_account_id
         AND o.quantity>(SELECT COALESCE(SUM(a.quantity),0)
                         FROM government_bond_buyback_allocations a
                         WHERE a.order_id=o.order_id)
       ORDER BY o.order_id LIMIT 1 FOR UPDATE;
     IF v_order IS NULL THEN
       SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='buyback order allocation mismatch';
     END IF;
     SELECT COALESCE(SUM(quantity),0) INTO v_order_redeemed
       FROM government_bond_buyback_allocations WHERE order_id=v_order;
     SET v_slice=LEAST(v_remaining,v_order_qty-v_order_redeemed);
     INSERT IGNORE INTO government_bond_coupon_positions(order_id,issue_id,account_id)
       VALUES(v_order,v_issue,p_account_id);
     SELECT accrued,frozen_accrued,last_hour_key
       INTO v_accrued,v_frozen,v_last_hour
       FROM government_bond_coupon_positions WHERE order_id=v_order FOR UPDATE;
     SET v_p_hour=TIMESTAMPDIFF(HOUR,'1970-01-01 00:00:00',v_purchase);
     SET v_m_hour=TIMESTAMPDIFF(HOUR,'1970-01-01 00:00:00',v_maturity);
     SET v_active_coupon=0;
     IF v_last_hour>v_p_hour AND v_m_hour>v_p_hour THEN
       SET v_active_coupon=
         ((CAST(v_face AS UNSIGNED)*v_yield DIV 10000)
          *(v_order_qty-v_order_redeemed-v_slice)
          *LEAST(v_last_hour-v_p_hour,v_m_hour-v_p_hour))
          DIV (v_m_hour-v_p_hour);
     END IF;
     IF v_accrued<v_frozen+v_active_coupon THEN
       SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='coupon freeze mismatch';
     END IF;
     SET v_new_frozen=v_accrued-v_frozen-v_active_coupon;
     UPDATE government_bond_coupon_positions
       SET frozen_accrued=frozen_accrued+v_new_frozen WHERE order_id=v_order;
     INSERT INTO government_bond_buyback_allocations
       (buyback_id,order_id,quantity,frozen_coupon_amount)
       VALUES(v_existing,v_order,v_slice,v_new_frozen);
     SET v_remaining=v_remaining-v_slice;
   END WHILE;
   IF p_quantity=v_held THEN
     DELETE FROM government_bond_holdings
       WHERE issue_id=v_issue AND account_id=p_account_id;
   ELSE
     UPDATE government_bond_holdings
       SET quantity=quantity-p_quantity,row_version=row_version+1
       WHERE issue_id=v_issue AND account_id=p_account_id;
   END IF;
   UPDATE government_bond_issues
     SET bought_back_quantity=bought_back_quantity+p_quantity,
         row_version=row_version+1 WHERE issue_id=v_issue;
   UPDATE accounts SET balance=balance+v_payment,row_version=row_version+1,
      txn_serial=txn_serial+1 WHERE account_id=p_account_id;
   UPDATE treasury_accounts SET balance=balance-v_payment,
      bond_fund_committed=bond_fund_committed-v_release,
      row_version=row_version+1 WHERE code='state';
   IF v_release>0 THEN
     UPDATE bond_fund_guarantees SET released_exposure=released_exposure+v_release
       WHERE issue_code=v_code;
     INSERT INTO bond_fund_ledger
       (event_key,operation,reference_code,reserve_delta,commitment_delta,
        reserved_after,committed_after,treasury_row_version_after)
       VALUES(CONCAT('fund:',v_existing),'RELEASE',v_code,0,-CAST(v_release AS SIGNED),
              v_fund,v_committed-v_release,v_treasury_version+1);
   END IF;
   INSERT INTO treasury_ledger
      (event_key,amount_delta,balance_after,source,actor_account_id,note)
      VALUES(p_event_key,-CAST(v_payment AS SIGNED),v_balance-v_payment,
             'bond_buyback',p_account_id,v_code);
 END IF;
 SELECT balance,row_version,txn_serial
   INTO v_account,v_account_version,v_serial FROM accounts WHERE account_id=p_account_id;
 SELECT balance,row_version INTO v_balance,v_treasury_version
   FROM treasury_accounts WHERE code='state';
 COMMIT;
 SELECT v_issue AS issue_id,v_payment AS buyback_paid,
        v_account AS account_balance,v_account_version AS account_row_version,
        v_serial AS account_txn_serial,v_balance AS treasury_balance,
        v_treasury_version AS treasury_row_version,v_replayed AS replayed;
END$$

DROP PROCEDURE IF EXISTS gov_bond_coupon_accrue$$
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
    p_hour_key>UNIX_TIMESTAMP() DIV 3600 THEN
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
     SET p.accrued=p.frozen_accrued+
        (CAST((v_face*v_yield DIV 10000)*(o.quantity-
          COALESCE((SELECT SUM(a.quantity) FROM government_bond_buyback_allocations a
                    WHERE a.order_id=o.order_id),0)) AS DECIMAL(30,0))*
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
   -- Materialize ONLY the positive increase after a successful PayDay.
   -- Notices are non-financial; each issue/account/hour is unique.
   INSERT INTO government_bond_coupon_notice_queue
     (issue_id,account_id,hour_key,amount)
     SELECT p.issue_id,p.account_id,p_hour_key,
       SUM(p.accrued)-COALESCE(MAX(w.accrued),0)
     FROM government_bond_coupon_positions p
     LEFT JOIN government_bond_coupon_wallets w
       ON w.issue_id=p.issue_id AND w.account_id=p.account_id
     WHERE p.issue_id=v_issue
     GROUP BY p.issue_id,p.account_id
     HAVING SUM(p.accrued)>COALESCE(MAX(w.accrued),0);
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
 DECLARE v_boughtback INT UNSIGNED;
 DECLARE v_bad INT UNSIGNED DEFAULT 0;
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
 SELECT issue_id,status,sold_quantity,bought_back_quantity,face_value,yield_basis_points,
        maturity_at,maturity_event_key
   INTO v_issue,v_status,v_sold,v_boughtback,v_face,v_yield,v_maturity,v_key
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
   IF v_count + v_boughtback <> v_sold THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond holdings quantity mismatch';
   END IF;
   -- Interest entitlement: active full-term coupon + frozen coupon
   -- earned on sold units, minus coupons already collected by that account.
   -- Claims for sold-out holders may have zero principal.
   SELECT COUNT(*) INTO v_bad
     FROM (SELECT account_id FROM government_bond_holdings WHERE issue_id=v_issue
           UNION SELECT account_id FROM government_bond_coupon_wallets WHERE issue_id=v_issue) owners
     LEFT JOIN government_bond_holdings h ON h.issue_id=v_issue AND h.account_id=owners.account_id
     LEFT JOIN government_bond_coupon_wallets w ON w.issue_id=v_issue AND w.account_id=owners.account_id
     LEFT JOIN (SELECT account_id,SUM(frozen_accrued) AS frozen FROM government_bond_coupon_positions
                WHERE issue_id=v_issue GROUP BY account_id) f ON f.account_id=owners.account_id
     WHERE (CAST(v_face AS UNSIGNED)*v_yield DIV 10000)*COALESCE(h.quantity,0)
           +COALESCE(f.frozen,0)<COALESCE(w.collected,0);
   IF v_bad<>0 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='coupon ownership entitlement mismatch';
   END IF;
   IF v_enabled=1 AND v_expense_enabled=1 AND
      v_balance >= v_minimum+v_fund+v_pool THEN
     SET v_free = v_balance-v_minimum-v_fund-v_pool;
   END IF;
   -- Uncollected sold-unit coupons are converted into a claim, not lost.
   UPDATE government_bond_coupon_wallets
     SET converted=accrued-collected WHERE issue_id=v_issue;
   INSERT INTO government_bond_claims(issue_id,account_id,principal_due,interest_due)
     SELECT v_issue,owners.account_id,
         CAST(v_face AS UNSIGNED)*COALESCE(h.quantity,0),
         (CAST(v_face AS UNSIGNED)*v_yield DIV 10000)*COALESCE(h.quantity,0)
         +COALESCE(f.frozen,0)-COALESCE(w.collected,0)
     FROM (SELECT account_id FROM government_bond_holdings WHERE issue_id=v_issue
           UNION SELECT account_id FROM government_bond_coupon_wallets WHERE issue_id=v_issue) owners
     LEFT JOIN government_bond_holdings h ON h.issue_id=v_issue AND h.account_id=owners.account_id
     LEFT JOIN government_bond_coupon_wallets w ON w.issue_id=v_issue AND w.account_id=owners.account_id
     LEFT JOIN (SELECT account_id,SUM(frozen_accrued) AS frozen FROM government_bond_coupon_positions
                WHERE issue_id=v_issue GROUP BY account_id) f ON f.account_id=owners.account_id
     WHERE COALESCE(h.quantity,0)>0 OR COALESCE(f.frozen,0)>COALESCE(w.collected,0);
   SELECT COALESCE(SUM(principal_due),0),COALESCE(SUM(interest_due),0)
     INTO v_principal,v_interest FROM government_bond_claims WHERE issue_id=v_issue;
   SET v_principal_budget=LEAST(v_free,v_principal);
   SET v_interest_budget=LEAST(GREATEST(0,v_free-v_principal_budget),v_interest);
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

DROP PROCEDURE IF EXISTS gov_bond_insurance_assess$$
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
 DECLARE v_remaining BIGINT UNSIGNED DEFAULT NULL;
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
   IF v_exposure IS NULL OR v_released>v_exposure OR v_committed < v_exposure-v_released THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='guarantee missing or already released';
   END IF;
   SET v_remaining=v_exposure-v_released;
   UPDATE government_bond_claims SET insurance_awarded=
     ((CAST(principal_due-principal_budget_awarded AS DECIMAL(30,0))
        *v_coverage) DIV 10000) WHERE issue_id=v_issue;
   SELECT COALESCE(SUM(insurance_awarded),0),
          COALESCE(SUM(principal_due+interest_due-
            principal_budget_awarded-interest_budget_awarded-insurance_awarded),0)
     INTO v_amount,v_remaining_due
     FROM government_bond_claims WHERE issue_id=v_issue;
   IF v_amount>v_remaining OR v_amount>v_fund OR
      v_pool+v_amount>2000000000 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='insurance coverage overdraw';
   END IF;
   UPDATE treasury_accounts SET
      bond_fund_reserved=bond_fund_reserved-v_amount,
      bond_fund_committed=bond_fund_committed-v_remaining,
      bond_payout_reserved=bond_payout_reserved+v_amount,
      row_version=row_version+1 WHERE code='state';
   SET v_fund=v_fund-v_amount;
   SET v_committed=v_committed-v_remaining;
   SET v_pool=v_pool+v_amount;
   SET v_version=v_version+1;
   UPDATE bond_fund_guarantees
      SET released_exposure=max_exposure WHERE issue_code=p_issue_code;
   IF v_remaining>0 OR v_amount>0 THEN
     INSERT INTO bond_fund_ledger
       (event_key,operation,reference_code,reserve_delta,commitment_delta,
        reserved_after,committed_after,treasury_row_version_after)
       VALUES(p_event_key,'INSURANCE_ALLOCATE',p_issue_code,-v_amount,-v_remaining,
              v_fund,v_committed,v_version);
   END IF;
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

DROP PROCEDURE IF EXISTS gov_finance_assert_integrity$$
CREATE PROCEDURE gov_finance_assert_integrity()
SQL SECURITY DEFINER
BEGIN
 DECLARE v_balance BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_fund BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_committed BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_payout BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_min BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_fund_util SMALLINT UNSIGNED DEFAULT 0;
 DECLARE v_treasury_ledger BIGINT DEFAULT 0;
 DECLARE v_fund_ledger BIGINT DEFAULT 0;
 DECLARE v_commit_ledger BIGINT DEFAULT 0;
 DECLARE v_payout_ledger BIGINT DEFAULT 0;
 DECLARE v_claim_escrow DECIMAL(30,0) DEFAULT 0;
 DECLARE v_quantity_mismatches BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_owner_mismatches BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_buyback_mismatches BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_alloc_mismatches BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_buyback_audit_mismatches BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_receipt_mismatches BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_order_mismatches BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_broken_claims BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_ledger_count BIGINT UNSIGNED DEFAULT 0;

 SELECT balance,bond_fund_reserved,bond_fund_committed,
        bond_payout_reserved,minimum_reserve,bond_fund_utilization_bps
   INTO v_balance,v_fund,v_committed,v_payout,v_min,v_fund_util
   FROM treasury_accounts WHERE code='state';
 IF v_balance IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='GOV-06 missing unique state treasury';
 END IF;
 SELECT COALESCE(SUM(amount_delta),0),COUNT(*)
   INTO v_treasury_ledger,v_ledger_count FROM treasury_ledger;
 SELECT COALESCE(SUM(reserve_delta),0),COALESCE(SUM(commitment_delta),0)
   INTO v_fund_ledger,v_commit_ledger FROM bond_fund_ledger;
 SELECT COALESCE(SUM(reserved_delta),0)
   INTO v_payout_ledger FROM bond_payout_ledger;
 SELECT COALESCE(SUM(principal_budget_awarded+interest_budget_awarded+
                     insurance_awarded-amount_collected),0)
   INTO v_claim_escrow FROM government_bond_claims;
 SELECT COUNT(*) INTO v_quantity_mismatches
 FROM government_bond_issues i
 WHERE i.sold_quantity<>(SELECT COALESCE(SUM(o.quantity),0)
                             FROM government_bond_orders o WHERE o.issue_id=i.issue_id);
 SELECT COUNT(*) INTO v_owner_mismatches
 FROM government_bond_issues i
 WHERE (SELECT COALESCE(SUM(h.quantity),0)
          FROM government_bond_holdings h WHERE h.issue_id=i.issue_id)
       + (SELECT COALESCE(SUM(b.quantity),0)
             FROM government_bond_buybacks b WHERE b.issue_id=i.issue_id)
       <> (SELECT COALESCE(SUM(o.quantity),0)
             FROM government_bond_orders o WHERE o.issue_id=i.issue_id);
 SELECT COUNT(*) INTO v_buyback_mismatches FROM government_bond_issues i
 WHERE i.bought_back_quantity<>(SELECT COALESCE(SUM(b.quantity),0)
                                    FROM government_bond_buybacks b WHERE b.issue_id=i.issue_id);
 SELECT COUNT(*) INTO v_alloc_mismatches FROM government_bond_orders o
 WHERE (SELECT COALESCE(SUM(a.quantity),0) FROM government_bond_buyback_allocations a
        WHERE a.order_id=o.order_id)>o.quantity
    OR (SELECT COALESCE(SUM(a.frozen_coupon_amount),0)
          FROM government_bond_buyback_allocations a WHERE a.order_id=o.order_id)
       <> (SELECT COALESCE(MAX(p.frozen_accrued),0)
           FROM government_bond_coupon_positions p WHERE p.order_id=o.order_id);
 SELECT COUNT(*) INTO v_buyback_audit_mismatches FROM government_bond_buybacks b
 LEFT JOIN treasury_ledger t ON t.event_key=b.event_key
 WHERE t.ledger_id IS NULL OR t.source<>'bond_buyback'
       OR t.amount_delta<>-CAST(b.amount_paid AS SIGNED)
       OR t.actor_account_id<>b.account_id
       OR b.amount_paid<>CAST(b.unit_price AS UNSIGNED)*b.quantity
       OR b.quantity<>(SELECT COALESCE(SUM(a.quantity),0)
                       FROM government_bond_buyback_allocations a WHERE a.buyback_id=b.buyback_id);
 SELECT COUNT(*) INTO v_receipt_mismatches
 FROM government_bond_claims c
 WHERE c.amount_collected<>(SELECT COALESCE(SUM(r.amount),0)
                               FROM government_bond_claim_receipts r WHERE r.claim_id=c.claim_id);
 SELECT COUNT(*) INTO v_order_mismatches
 FROM government_bond_orders o
 LEFT JOIN treasury_ledger t ON t.event_key=o.event_key
 WHERE t.ledger_id IS NULL OR t.amount_delta<>o.amount_paid OR
       t.source<>'bond_placement' OR t.actor_account_id<>o.account_id;
 SELECT COUNT(*) INTO v_broken_claims
 FROM government_bond_claims c
 WHERE c.amount_collected>c.principal_budget_awarded+c.interest_budget_awarded+
       c.insurance_awarded OR
       c.insurance_awarded>c.principal_due-c.principal_budget_awarded;

 IF v_balance<v_min+v_fund+v_payout OR
    CAST(v_committed AS DECIMAL(30,0))*10000 >
      CAST(v_fund AS DECIMAL(30,0))*v_fund_util THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='GOV-06 protected reserve invariant violation';
 END IF;
 IF v_balance<>v_treasury_ledger OR v_fund<>v_fund_ledger OR
    v_committed<>v_commit_ledger OR v_payout<>v_payout_ledger OR
    v_payout<>v_claim_escrow THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='GOV-06 ledger balance reconciliation failed';
 END IF;
 IF v_quantity_mismatches<>0 OR v_owner_mismatches<>0 OR
    v_buyback_mismatches<>0 OR v_alloc_mismatches<>0 OR
    v_buyback_audit_mismatches<>0 OR
    v_receipt_mismatches<>0 OR v_order_mismatches<>0 OR v_broken_claims<>0 THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='GOV-06 issuance or investor ownership mismatch';
 END IF;
 SELECT 'PASS' AS result,v_balance AS treasury_balance,
   v_fund AS insurance_reserved,v_committed AS insurance_committed,
   v_payout AS investor_escrow,v_ledger_count AS treasury_transactions,
   (SELECT COUNT(*) FROM government_bond_issues) AS total_issues,
   (SELECT COUNT(*) FROM government_bond_orders) AS total_orders,
   (SELECT COUNT(*) FROM government_bond_claim_receipts) AS total_claim_receipts;
END$$
DELIMITER ;
INSERT INTO schema_migrations(version) VALUES (30);
