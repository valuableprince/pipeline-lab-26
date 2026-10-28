-- GOV-04: overdue bond recovery, transparent state fiscal stress and non-haircut restructuring.
-- Apply once after 021, only with the game server stopped. No minted cash, no offline wallet writes.
-- Monetary allocations remain inside the single treasury as the protected bond_payout_reserved escrow.
-- Obligations are immutable: no changing original face/yield/insurance/claims when restructuring.

ALTER TABLE treasury_accounts
  ADD COLUMN bond_crisis_state VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin
    NOT NULL DEFAULT 'NORMAL',
  ADD COLUMN bond_crisis_version INT UNSIGNED NOT NULL DEFAULT 0,
  ADD CONSTRAINT chk_bond_crisis_state CHECK
    (bond_crisis_state IN ('NORMAL','STRESSED','DEFAULT','RECOVERY')),
  ADD CONSTRAINT chk_bond_crisis_version CHECK (bond_crisis_version<=2147483646);

ALTER TABLE government_bond_issues
  ADD COLUMN recovery_round INT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN last_recovery_at DATETIME NULL,
  ADD COLUMN restructure_revision INT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN restructure_pending_plan BIGINT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN debt_target_at DATETIME NULL,
  ADD CONSTRAINT chk_bond_debt_revisions CHECK
    (recovery_round<=2147483646 AND restructure_revision<=2147483646);

ALTER TABLE bond_payout_ledger
  DROP CONSTRAINT chk_bond_payout_event,
  ADD CONSTRAINT chk_bond_payout_event CHECK
    (operation IN ('MATURITY','INSURANCE','COLLECT','RECOVERY') AND
     (reserved_delta <> 0 OR operation IN ('MATURITY','INSURANCE')));

CREATE TABLE government_bond_recoveries (
  recovery_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  issue_id BIGINT UNSIGNED NOT NULL,
  recovery_round INT UNSIGNED NOT NULL,
  budget_cap BIGINT UNSIGNED NOT NULL,
  principal_allocated BIGINT UNSIGNED NOT NULL,
  interest_allocated BIGINT UNSIGNED NOT NULL,
  payout_reserved_after BIGINT UNSIGNED NOT NULL,
  treasury_row_version_after INT UNSIGNED NOT NULL,
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  UNIQUE KEY uq_bond_recovery_event (event_key),
  UNIQUE KEY uq_bond_recovery_round (issue_id,recovery_round),
  CONSTRAINT fk_bond_recovery_issue FOREIGN KEY (issue_id)
    REFERENCES government_bond_issues(issue_id) ON DELETE RESTRICT,
  CONSTRAINT chk_bond_recovery_amounts CHECK
    (budget_cap BETWEEN 1 AND 1000000000 AND
     principal_allocated+interest_allocated BETWEEN 1 AND budget_cap)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE government_bond_restructures (
  plan_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  issue_id BIGINT UNSIGNED NOT NULL,
  revision INT UNSIGNED NOT NULL,
  propose_event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  approve_event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  proposed_by_account_id BIGINT UNSIGNED NOT NULL,
  approved_by_account_id BIGINT UNSIGNED NULL,
  target_repayment_at DATETIME NOT NULL,
  rationale VARCHAR(512) NOT NULL,
  original_maturity_at DATETIME NOT NULL,
  status VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'PROPOSED',
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  approved_at DATETIME NULL,
  UNIQUE KEY uq_bond_restructure_revision (issue_id,revision),
  UNIQUE KEY uq_bond_restructure_proposal (propose_event_key),
  UNIQUE KEY uq_bond_restructure_approval (approve_event_key),
  CONSTRAINT fk_bond_restructure_issue FOREIGN KEY (issue_id)
    REFERENCES government_bond_issues(issue_id) ON DELETE RESTRICT,
  CONSTRAINT fk_bond_restructure_creator FOREIGN KEY (proposed_by_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT fk_bond_restructure_approver FOREIGN KEY (approved_by_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_bond_restructure_state CHECK
    (status IN ('PROPOSED','APPROVED','SUPERSEDED')),
  CONSTRAINT chk_bond_restructure_actors CHECK
    (approved_by_account_id IS NULL OR approved_by_account_id<>proposed_by_account_id),
  CONSTRAINT chk_bond_restructure_approval CHECK
    ((status='PROPOSED' AND approved_by_account_id IS NULL AND
      approve_event_key IS NULL AND approved_at IS NULL)
     OR (status IN ('APPROVED','SUPERSEDED') AND approved_by_account_id IS NOT NULL
      AND approve_event_key IS NOT NULL AND approved_at IS NOT NULL))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE government_bond_restructure_audit (
  audit_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  plan_id BIGINT UNSIGNED NOT NULL,
  action VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  actor_account_id BIGINT UNSIGNED NOT NULL,
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  UNIQUE KEY uq_bond_restructure_audit_event (event_key),
  CONSTRAINT fk_bond_restructure_audit_plan FOREIGN KEY (plan_id)
    REFERENCES government_bond_restructures(plan_id) ON DELETE RESTRICT,
  CONSTRAINT fk_bond_restructure_audit_actor FOREIGN KEY (actor_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_bond_restructure_audit_action CHECK
    (action IN ('PROPOSE','APPROVE'))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE bond_crisis_events (
  crisis_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  previous_state VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  current_state VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  defaulted_issues INT UNSIGNED NOT NULL,
  restructured_issues INT UNSIGNED NOT NULL,
  due_issues INT UNSIGNED NOT NULL,
  outstanding_amount BIGINT UNSIGNED NOT NULL,
  treasury_row_version_after INT UNSIGNED NOT NULL,
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  UNIQUE KEY uq_bond_crisis_event (event_key),
  CONSTRAINT chk_bond_crisis_transition CHECK
    (previous_state<>current_state AND
     previous_state IN ('NORMAL','STRESSED','DEFAULT','RECOVERY') AND
     current_state IN ('NORMAL','STRESSED','DEFAULT','RECOVERY'))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DELIMITER $$
-- Proportional stable rounding uses cumulative allocations per owner, so the
-- entire available integer amount is assigned with no rounding leakage.
-- A claim can collect concurrently only AFTER this transaction releases treasury.
CREATE PROCEDURE gov_bond_recovery_allocate(
 IN p_event_key VARCHAR(64), IN p_issue_code VARCHAR(64), IN p_budget_cap INT
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_balance BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_fund BIGINT UNSIGNED;
 DECLARE v_pool BIGINT UNSIGNED;
 DECLARE v_min BIGINT UNSIGNED;
 DECLARE v_version INT UNSIGNED;
 DECLARE v_enabled TINYINT UNSIGNED;
 DECLARE v_expenses TINYINT UNSIGNED;
 DECLARE v_max_tx BIGINT UNSIGNED;
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_debt VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_insurance_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_round INT UNSIGNED;
 DECLARE v_old_id BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_old_issue BIGINT UNSIGNED;
 DECLARE v_old_cap BIGINT UNSIGNED;
 DECLARE v_principal_total BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_interest_total BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_free BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_budget BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_principal_budget BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_interest_budget BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_principal_seen BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_interest_seen BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_principal_awarded BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_interest_awarded BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_principal_step BIGINT UNSIGNED;
 DECLARE v_interest_step BIGINT UNSIGNED;
 DECLARE v_claim BIGINT UNSIGNED;
 DECLARE v_claim_principal BIGINT UNSIGNED;
 DECLARE v_claim_interest BIGINT UNSIGNED;
 DECLARE v_done TINYINT UNSIGNED DEFAULT 0;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE cur CURSOR FOR
  SELECT claim_id,principal_due-principal_budget_awarded-insurance_awarded,
         interest_due-interest_budget_awarded
    FROM government_bond_claims WHERE issue_id=v_issue ORDER BY claim_id;
 DECLARE CONTINUE HANDLER FOR NOT FOUND SET v_done=1;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;

 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
    OR LEFT(BINARY p_event_key,5) <> BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_issue_code IS NULL OR CHAR_LENGTH(p_issue_code) NOT BETWEEN 1 AND 64
    OR p_issue_code NOT REGEXP '^[A-Za-z0-9_:/.-]+$'
    OR p_budget_cap IS NULL OR p_budget_cap<1 OR p_budget_cap>1000000000 THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid overdue recovery request';
 END IF;
 START TRANSACTION;
 SELECT balance,bond_fund_reserved,bond_payout_reserved,minimum_reserve,
        row_version,enabled,expense_enabled,max_transaction
   INTO v_balance,v_fund,v_pool,v_min,v_version,v_enabled,v_expenses,v_max_tx
   FROM treasury_accounts WHERE code='state' FOR UPDATE;
 SELECT issue_id,debt_status,insurance_event_key,recovery_round
   INTO v_issue,v_debt,v_insurance_key,v_round
   FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE;
 IF v_balance IS NULL OR v_issue IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='overdue issue or treasury missing';
 END IF;
 SELECT recovery_id,issue_id,budget_cap
   INTO v_old_id,v_old_issue,v_old_cap
   FROM government_bond_recoveries WHERE event_key=p_event_key LIMIT 1;
 IF v_old_id IS NOT NULL THEN
   IF v_old_issue<>v_issue OR v_old_cap<>p_budget_cap THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='recovery event collision';
   END IF;
   SET v_replayed=1;
 ELSE
   IF v_debt NOT IN ('DEFAULTED','RESTRUCTURED') OR
      v_insurance_key IS NULL OR v_round>=2147483646 OR
      v_version>=2147483646 OR v_enabled<>1 OR v_expenses<>1 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='recovery not authorized by issue state';
   END IF;
   IF v_balance>=v_min+v_fund+v_pool THEN
     SET v_free=v_balance-v_min-v_fund-v_pool;
   END IF;
   SELECT COALESCE(SUM(principal_due-principal_budget_awarded-insurance_awarded),0),
          COALESCE(SUM(interest_due-interest_budget_awarded),0)
     INTO v_principal_total,v_interest_total
     FROM government_bond_claims WHERE issue_id=v_issue;
   SET v_budget=LEAST(v_free,p_budget_cap,v_max_tx,
                       v_principal_total+v_interest_total);
   IF v_budget=0 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='no free budget for overdue bond';
   END IF;
   SET v_principal_budget=LEAST(v_budget,v_principal_total);
   SET v_interest_budget=v_budget-v_principal_budget;
   SET v_done=0;
   OPEN cur;
   claims_loop: LOOP
     FETCH cur INTO v_claim,v_claim_principal,v_claim_interest;
     IF v_done=1 THEN LEAVE claims_loop; END IF;
     SET v_principal_step=0;
     SET v_interest_step=0;
     IF v_principal_total>0 THEN
       SET v_principal_step=
         (CAST(v_principal_seen+v_claim_principal AS DECIMAL(30,0))*
           v_principal_budget DIV v_principal_total)
         -(CAST(v_principal_seen AS DECIMAL(30,0))*
           v_principal_budget DIV v_principal_total);
     END IF;
     IF v_interest_total>0 THEN
       SET v_interest_step=
         (CAST(v_interest_seen+v_claim_interest AS DECIMAL(30,0))*
           v_interest_budget DIV v_interest_total)
         -(CAST(v_interest_seen AS DECIMAL(30,0))*
           v_interest_budget DIV v_interest_total);
     END IF;
     IF v_principal_step>0 OR v_interest_step>0 THEN
       UPDATE government_bond_claims SET
         principal_budget_awarded=principal_budget_awarded+v_principal_step,
         interest_budget_awarded=interest_budget_awarded+v_interest_step
         WHERE claim_id=v_claim;
     END IF;
     SET v_principal_seen=v_principal_seen+v_claim_principal;
     SET v_interest_seen=v_interest_seen+v_claim_interest;
     SET v_principal_awarded=v_principal_awarded+v_principal_step;
     SET v_interest_awarded=v_interest_awarded+v_interest_step;
   END LOOP;
   CLOSE cur;
   IF v_principal_seen<>v_principal_total OR v_interest_seen<>v_interest_total
      OR v_principal_awarded<>v_principal_budget
      OR v_interest_awarded<>v_interest_budget THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='recovery allocation reconciliation failed';
   END IF;
   UPDATE treasury_accounts
      SET bond_payout_reserved=bond_payout_reserved+v_budget,
          row_version=row_version+1 WHERE code='state';
   SET v_pool=v_pool+v_budget;
   SET v_version=v_version+1;
   INSERT INTO bond_payout_ledger
      (event_key,operation,issue_id,reserved_delta,reserved_after,treasury_row_version_after)
      VALUES(p_event_key,'RECOVERY',v_issue,v_budget,v_pool,v_version);
   UPDATE government_bond_issues
      SET recovery_round=recovery_round+1,last_recovery_at=UTC_TIMESTAMP(),
          debt_status=IF(v_principal_total+v_interest_total=v_budget,
                         'SETTLED',v_debt),row_version=row_version+1
      WHERE issue_id=v_issue;
   INSERT INTO government_bond_recoveries
      (event_key,issue_id,recovery_round,budget_cap,principal_allocated,
       interest_allocated,payout_reserved_after,treasury_row_version_after)
      VALUES(p_event_key,v_issue,v_round+1,p_budget_cap,v_principal_budget,
             v_interest_budget,v_pool,v_version);
 END IF;
 COMMIT;
 SELECT v_issue AS issue_id,v_principal_budget AS principal_allocated,
        v_interest_budget AS interest_allocated,v_pool AS payout_reserved,
        v_replayed AS replayed;
END$$

-- A voluntary restructuring sets a target date ONLY, no principal haircut,
-- no resetting the original maturity clock, and NO waiver of investor claims.
CREATE PROCEDURE gov_bond_restructure_propose(
 IN p_event_key VARCHAR(64),IN p_issue_code VARCHAR(64),
 IN p_target_at DATETIME,IN p_rationale VARCHAR(512),
 IN p_actor BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_debt VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_revision INT UNSIGNED;
 DECLARE v_pending BIGINT UNSIGNED;
 DECLARE v_maturity DATETIME;
 DECLARE v_insurance_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_outstanding BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_plan BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_code VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_target DATETIME;
 DECLARE v_rationale VARCHAR(512);
 DECLARE v_actor BIGINT UNSIGNED;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
    OR LEFT(BINARY p_event_key,5)<>BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_issue_code IS NULL OR CHAR_LENGTH(p_issue_code) NOT BETWEEN 1 AND 64
    OR p_issue_code NOT REGEXP '^[A-Za-z0-9_:/.-]+$'
    OR p_target_at IS NULL OR p_rationale IS NULL
    OR CHAR_LENGTH(TRIM(p_rationale)) NOT BETWEEN 10 AND 512
    OR p_actor IS NULL OR p_actor=0 THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid restructure proposal';
 END IF;
 START TRANSACTION;
 SELECT issue_id,debt_status,restructure_revision,restructure_pending_plan,
        maturity_at,insurance_event_key
    INTO v_issue,v_debt,v_revision,v_pending,v_maturity,v_insurance_key
    FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE;
 IF v_issue IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='restructure issue missing';
 END IF;
 SELECT plan_id,target_repayment_at,rationale,proposed_by_account_id
   INTO v_plan,v_target,v_rationale,v_actor
   FROM government_bond_restructures WHERE propose_event_key=p_event_key LIMIT 1;
 IF v_plan IS NOT NULL THEN
   IF v_target<>p_target_at OR BINARY v_rationale<>BINARY p_rationale OR
      v_actor<>p_actor THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='restructure proposal collision';
   END IF;
   SET v_replayed=1;
 ELSE
   IF v_debt NOT IN ('DEFAULTED','RESTRUCTURED') OR
      v_insurance_key IS NULL OR v_pending<>0 OR
      v_revision>=2147483646 OR p_target_at<=UTC_TIMESTAMP() OR
      p_target_at>DATE_ADD(UTC_TIMESTAMP(),INTERVAL 180 DAY) OR
      p_target_at<=v_maturity THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='restructure proposal not allowed';
   END IF;
   SELECT COALESCE(SUM(principal_due+interest_due-
       principal_budget_awarded-interest_budget_awarded-insurance_awarded),0)
     INTO v_outstanding FROM government_bond_claims WHERE issue_id=v_issue;
   IF v_outstanding=0 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='restructure has no outstanding debt';
   END IF;
   INSERT INTO government_bond_restructures
      (issue_id,revision,propose_event_key,proposed_by_account_id,
       target_repayment_at,rationale,original_maturity_at)
      VALUES(v_issue,v_revision+1,p_event_key,p_actor,p_target_at,
             p_rationale,v_maturity);
   SET v_plan=LAST_INSERT_ID();
   UPDATE government_bond_issues SET
      restructure_pending_plan=v_plan,row_version=row_version+1 WHERE issue_id=v_issue;
   INSERT INTO government_bond_restructure_audit
      (event_key,plan_id,action,actor_account_id)
      VALUES(p_event_key,v_plan,'PROPOSE',p_actor);
 END IF;
 COMMIT;
 SELECT v_plan AS plan_id,v_issue AS issue_id,v_replayed AS replayed;
END$$

CREATE PROCEDURE gov_bond_restructure_approve(
 IN p_event_key VARCHAR(64),IN p_issue_code VARCHAR(64),
 IN p_plan_id BIGINT UNSIGNED,IN p_actor BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_pending BIGINT UNSIGNED;
 DECLARE v_revision INT UNSIGNED;
 DECLARE v_debt VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_plan BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_plan_issue BIGINT UNSIGNED;
 DECLARE v_plan_revision INT UNSIGNED;
 DECLARE v_proposer BIGINT UNSIGNED;
 DECLARE v_approver BIGINT UNSIGNED;
 DECLARE v_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_status VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_target DATETIME;
 DECLARE v_remaining BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
    OR LEFT(BINARY p_event_key,5)<>BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_issue_code IS NULL OR CHAR_LENGTH(p_issue_code) NOT BETWEEN 1 AND 64
    OR p_issue_code NOT REGEXP '^[A-Za-z0-9_:/.-]+$'
    OR p_plan_id IS NULL OR p_plan_id=0 OR p_actor IS NULL OR p_actor=0 THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid restructure approval';
 END IF;
 START TRANSACTION;
 SELECT issue_id,restructure_pending_plan,restructure_revision,debt_status
   INTO v_issue,v_pending,v_revision,v_debt
   FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE;
 SELECT plan_id,issue_id,revision,proposed_by_account_id,
        approved_by_account_id,approve_event_key,status,target_repayment_at
   INTO v_plan,v_plan_issue,v_plan_revision,v_proposer,
        v_approver,v_key,v_status,v_target
   FROM government_bond_restructures WHERE plan_id=p_plan_id FOR UPDATE;
 IF v_issue IS NULL OR v_plan IS NULL OR v_issue<>v_plan_issue THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='restructure approval issue mismatch';
 END IF;
 IF v_key IS NOT NULL THEN
   IF BINARY v_key<>BINARY p_event_key OR v_approver<>p_actor THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='restructure approval already assigned';
   END IF;
   SET v_replayed=1;
 ELSE
   IF v_pending<>v_plan OR v_status<>'PROPOSED' OR
      v_proposer=p_actor OR v_plan_revision<>v_revision+1 OR
      v_debt NOT IN ('DEFAULTED','RESTRUCTURED') OR
      v_target<=UTC_TIMESTAMP() OR v_revision>=2147483646 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='restructure approval forbidden';
   END IF;
   SELECT COALESCE(SUM(principal_due+interest_due-
        principal_budget_awarded-interest_budget_awarded-insurance_awarded),0)
     INTO v_remaining FROM government_bond_claims WHERE issue_id=v_issue;
   IF v_remaining=0 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='no debt left to restructure';
   END IF;
   UPDATE government_bond_restructures SET status='SUPERSEDED'
     WHERE issue_id=v_issue AND status='APPROVED';
   UPDATE government_bond_restructures SET
     status='APPROVED',approved_by_account_id=p_actor,
     approve_event_key=p_event_key,approved_at=UTC_TIMESTAMP() WHERE plan_id=v_plan;
   UPDATE government_bond_issues SET
      restructure_pending_plan=0,restructure_revision=v_plan_revision,
      debt_target_at=v_target,debt_status='RESTRUCTURED',
      row_version=row_version+1 WHERE issue_id=v_issue;
   INSERT INTO government_bond_restructure_audit
      (event_key,plan_id,action,actor_account_id)
      VALUES(p_event_key,v_plan,'APPROVE',p_actor);
 END IF;
 COMMIT;
 SELECT v_plan AS plan_id,v_issue AS issue_id,v_target AS target_repayment_at,
        v_replayed AS replayed;
END$$

-- Public fiscal status is derived from unsettled government obligations.
-- No debt principal or investor entitlement changes here.
CREATE PROCEDURE gov_bond_crisis_reconcile(IN p_event_key VARCHAR(64))
SQL SECURITY INVOKER
BEGIN
 DECLARE v_treasury BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_version INT UNSIGNED;
 DECLARE v_crisis_revision INT UNSIGNED;
 DECLARE v_old VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_new VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_defaults INT UNSIGNED DEFAULT 0;
 DECLARE v_restructures INT UNSIGNED DEFAULT 0;
 DECLARE v_dues INT UNSIGNED DEFAULT 0;
 DECLARE v_outstanding BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE v_prior_id BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_prior_new VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
    OR LEFT(BINARY p_event_key,5)<>BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$' THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid fiscal status event';
 END IF;
 START TRANSACTION;
 SELECT balance,row_version,bond_crisis_version,bond_crisis_state
   INTO v_treasury,v_version,v_crisis_revision,v_old
   FROM treasury_accounts WHERE code='state' FOR UPDATE;
 IF v_treasury IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='treasury missing for crisis';
 END IF;
 SELECT crisis_id,current_state INTO v_prior_id,v_prior_new
   FROM bond_crisis_events WHERE event_key=p_event_key LIMIT 1;
 IF v_prior_id IS NOT NULL THEN
   SET v_replayed=1;
 ELSE
   SELECT COALESCE(SUM(i.debt_status='DEFAULTED'),0),
          COALESCE(SUM(i.debt_status='RESTRUCTURED'),0),
          COALESCE(SUM(i.debt_status='DUE'),0)
     INTO v_defaults,v_restructures,v_dues
     FROM government_bond_issues i
     WHERE i.debt_status IN ('DEFAULTED','RESTRUCTURED','DUE');
   SELECT COALESCE(SUM(principal_due+interest_due-
           principal_budget_awarded-interest_budget_awarded-insurance_awarded),0)
     INTO v_outstanding FROM government_bond_claims;
   IF v_defaults>0 THEN SET v_new='DEFAULT';
   ELSEIF v_restructures>0 THEN SET v_new='RECOVERY';
   ELSEIF v_dues>0 THEN SET v_new='STRESSED';
   ELSE SET v_new='NORMAL';
   END IF;
   IF v_old<>v_new THEN
     IF v_version>=2147483646 OR v_crisis_revision>=2147483646 THEN
       SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='crisis version limit';
     END IF;
     UPDATE treasury_accounts
       SET bond_crisis_state=v_new,bond_crisis_version=bond_crisis_version+1,
           row_version=row_version+1 WHERE code='state';
     SET v_version=v_version+1;
     INSERT INTO bond_crisis_events
       (event_key,previous_state,current_state,defaulted_issues,
        restructured_issues,due_issues,outstanding_amount,treasury_row_version_after)
       VALUES(p_event_key,v_old,v_new,v_defaults,v_restructures,v_dues,
              v_outstanding,v_version);
   END IF;
 END IF;
 COMMIT;
 SELECT COALESCE(v_new,v_prior_new,v_old) AS crisis_state,
        v_outstanding AS unpaid_claims,v_replayed AS replayed;
END$$
DELIMITER ;

INSERT INTO schema_migrations(version) VALUES (22);
