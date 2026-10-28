-- GOV-02 / migration 020: government bond issue lifecycle and ownership registry.
-- Install ONCE after 019; stop open.mp during DDL. No seeded money or bonds.
-- GOVERNANCE: routines are internal only until GOV-05 authorizes officials.
-- Issuance reserves fund exposure and creates approved issue in ONE transaction.

CREATE TABLE government_bond_issues (
  issue_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  issue_code VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  title VARCHAR(128) NOT NULL,
  purpose VARCHAR(512) NOT NULL,
  issue_type VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'GENERAL',
  face_value INT UNSIGNED NOT NULL,
  max_quantity INT UNSIGNED NOT NULL,
  sold_quantity INT UNSIGNED NOT NULL DEFAULT 0,
  yield_basis_points SMALLINT UNSIGNED NOT NULL,
  coverage_basis_points SMALLINT UNSIGNED NULL,
  premium_basis_points SMALLINT UNSIGNED NULL,
  grace_seconds INT UNSIGNED NULL,
  insured_max_exposure BIGINT UNSIGNED NOT NULL DEFAULT 0,
  offer_start_at DATETIME NOT NULL,
  offer_end_at DATETIME NOT NULL,
  maturity_at DATETIME NOT NULL,
  status VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'DRAFT',
  debt_status VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'NOT_DUE',
  create_event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  approve_event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  created_by_account_id BIGINT UNSIGNED NOT NULL,
  approved_by_account_id BIGINT UNSIGNED NULL,
  approved_at DATETIME NULL,
  opened_at DATETIME NULL,
  cancelled_at DATETIME NULL,
  rejected_at DATETIME NULL,
  rejected_by_account_id BIGINT UNSIGNED NULL,
  rejection_reason VARCHAR(255) NULL,
  row_version INT UNSIGNED NOT NULL DEFAULT 0,
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  UNIQUE KEY uq_government_bond_code (issue_code),
  UNIQUE KEY uq_government_bond_create (create_event_key),
  UNIQUE KEY uq_government_bond_approve (approve_event_key),
  KEY idx_government_bond_status_time (status, offer_end_at),
  CONSTRAINT fk_government_bond_creator FOREIGN KEY (created_by_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT fk_government_bond_approver FOREIGN KEY (approved_by_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT fk_government_bond_rejector FOREIGN KEY (rejected_by_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_government_bond_type CHECK (issue_type IN ('GENERAL','TARGETED','CRISIS')),
  CONSTRAINT chk_government_bond_status CHECK
    (status IN ('DRAFT','APPROVED','OFFERING','CLOSED','CANCELLED','REJECTED')),
  CONSTRAINT chk_government_bond_debt CHECK
    (debt_status IN ('NOT_DUE','DUE','PARTIALLY_PAID','OVERDUE','DEFAULTED','RESTRUCTURED','SETTLED')),
  CONSTRAINT chk_government_bond_amounts CHECK
    (face_value BETWEEN 1 AND 1000000000 AND max_quantity BETWEEN 1 AND 1000000
     AND CAST(face_value AS UNSIGNED) * max_quantity <= 1000000000
     AND sold_quantity <= max_quantity AND yield_basis_points <= 10000
     AND insured_max_exposure <= 1000000000 AND row_version <= 2147483646),
  CONSTRAINT chk_government_bond_dates CHECK
    (offer_start_at < offer_end_at AND offer_end_at < maturity_at),
  CONSTRAINT chk_government_bond_rejection CHECK (
    (status='REJECTED' AND rejected_at IS NOT NULL
      AND rejected_by_account_id IS NOT NULL AND
      CHAR_LENGTH(rejection_reason) BETWEEN 5 AND 255)
    OR (status<>'REJECTED' AND rejected_at IS NULL AND
      rejected_by_account_id IS NULL AND rejection_reason IS NULL)
  ),
  CONSTRAINT chk_government_bond_insurance CHECK (
    (status IN ('DRAFT','REJECTED') AND coverage_basis_points IS NULL AND
     premium_basis_points IS NULL AND grace_seconds IS NULL AND
     insured_max_exposure=0 AND approve_event_key IS NULL AND
     approved_by_account_id IS NULL AND approved_at IS NULL)
    OR status='CANCELLED'
    OR (coverage_basis_points BETWEEN 1 AND 10000 AND
        premium_basis_points <= 10000 AND grace_seconds BETWEEN 60 AND 2592000
        AND insured_max_exposure > 0 AND approve_event_key IS NOT NULL AND
        approved_by_account_id IS NOT NULL AND approved_at IS NOT NULL)
  )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Empty ownership registry at this phase. GOV-03 introduces purchases and payments.
CREATE TABLE government_bond_holdings (
  issue_id BIGINT UNSIGNED NOT NULL,
  account_id BIGINT UNSIGNED NOT NULL,
  quantity INT UNSIGNED NOT NULL,
  locked_quantity INT UNSIGNED NOT NULL DEFAULT 0,
  row_version INT UNSIGNED NOT NULL DEFAULT 0,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (issue_id, account_id),
  KEY idx_bond_holdings_owner (account_id, issue_id),
  CONSTRAINT fk_bond_holding_issue FOREIGN KEY (issue_id)
    REFERENCES government_bond_issues(issue_id) ON DELETE RESTRICT,
  CONSTRAINT fk_bond_holding_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_bond_holding_quantity CHECK
    (quantity BETWEEN 1 AND 1000000 AND locked_quantity <= quantity
     AND row_version <= 2147483646)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE government_bond_issue_audit (
  event_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  issue_id BIGINT UNSIGNED NOT NULL,
  action VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  actor_account_id BIGINT UNSIGNED NOT NULL,
  from_status VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NULL,
  to_status VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  fund_exposure_delta BIGINT NOT NULL DEFAULT 0,
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  UNIQUE KEY uq_bond_issue_event (event_key),
  KEY idx_bond_issue_audit_issue (issue_id, event_id),
  CONSTRAINT fk_bond_issue_audit_issue FOREIGN KEY (issue_id)
    REFERENCES government_bond_issues(issue_id) ON DELETE RESTRICT,
  CONSTRAINT fk_bond_issue_audit_actor FOREIGN KEY (actor_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_bond_issue_audit_action CHECK
    (action IN ('CREATE','APPROVE','OPEN','CANCEL','REJECT'))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- A guarantee can be fully released only for an issue without a single sale.
ALTER TABLE bond_fund_guarantees
  ADD COLUMN released_exposure BIGINT UNSIGNED NOT NULL DEFAULT 0,
  ADD CONSTRAINT chk_bond_fund_released CHECK (released_exposure <= max_exposure);
ALTER TABLE bond_fund_ledger
  DROP CONSTRAINT chk_bond_fund_event_operation,
  ADD CONSTRAINT chk_bond_fund_event_operation CHECK
    (operation IN ('CAPITALIZE','COMMIT','RELEASE'));

DELIMITER $$
-- After approval, immutable economic terms cannot be changed by ordinary SQL UPDATE.
CREATE TRIGGER government_bond_terms_immutable
BEFORE UPDATE ON government_bond_issues FOR EACH ROW
BEGIN
  IF OLD.status <> 'DRAFT' AND (
       BINARY OLD.issue_code <> BINARY NEW.issue_code
       OR BINARY OLD.title <> BINARY NEW.title
       OR BINARY OLD.purpose <> BINARY NEW.purpose
       OR BINARY OLD.issue_type <> BINARY NEW.issue_type
       OR OLD.face_value <> NEW.face_value
       OR OLD.max_quantity <> NEW.max_quantity
       OR OLD.yield_basis_points <> NEW.yield_basis_points
       OR OLD.coverage_basis_points <> NEW.coverage_basis_points
       OR OLD.premium_basis_points <> NEW.premium_basis_points
       OR OLD.grace_seconds <> NEW.grace_seconds
       OR OLD.insured_max_exposure <> NEW.insured_max_exposure
       OR OLD.offer_start_at <> NEW.offer_start_at
       OR OLD.offer_end_at <> NEW.offer_end_at
       OR OLD.maturity_at <> NEW.maturity_at
       OR BINARY OLD.create_event_key <> BINARY NEW.create_event_key
       OR NOT (BINARY OLD.approve_event_key <=> BINARY NEW.approve_event_key)
       OR OLD.created_by_account_id <> NEW.created_by_account_id
       OR NOT (OLD.approved_by_account_id <=> NEW.approved_by_account_id)
       OR NOT (OLD.approved_at <=> NEW.approved_at)
       OR NOT (OLD.rejected_at <=> NEW.rejected_at)
       OR NOT (OLD.rejected_by_account_id <=> NEW.rejected_by_account_id)
       OR NOT (BINARY OLD.rejection_reason <=> BINARY NEW.rejection_reason)
     ) THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='approved bond terms immutable';
  END IF;
END$$

CREATE PROCEDURE gov_bond_issue_create(
 IN p_event_key VARCHAR(64), IN p_issue_code VARCHAR(64),
 IN p_title VARCHAR(128), IN p_purpose VARCHAR(512),
 IN p_issue_type VARCHAR(16), IN p_face_value INT,
 IN p_max_quantity INT, IN p_yield_bps INT,
 IN p_offer_start DATETIME, IN p_offer_end DATETIME,
 IN p_maturity DATETIME, IN p_actor_id BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_code VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_title VARCHAR(128);
 DECLARE v_purpose VARCHAR(512);
 DECLARE v_type VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_face INT UNSIGNED;
 DECLARE v_quantity INT UNSIGNED;
 DECLARE v_yield SMALLINT UNSIGNED;
 DECLARE v_start DATETIME;
 DECLARE v_end DATETIME;
 DECLARE v_maturity DATETIME;
 DECLARE v_actor BIGINT UNSIGNED;
 DECLARE v_state VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;

 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 6 AND 64
    OR LEFT(BINARY p_event_key,5) <> BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_issue_code IS NULL OR CHAR_LENGTH(p_issue_code) NOT BETWEEN 1 AND 64
    OR p_issue_code NOT REGEXP '^[A-Za-z0-9_:/.-]+$'
    OR p_title IS NULL OR CHAR_LENGTH(TRIM(p_title)) NOT BETWEEN 3 AND 128
    OR p_purpose IS NULL OR CHAR_LENGTH(TRIM(p_purpose)) NOT BETWEEN 3 AND 512
    OR p_issue_type IS NULL OR p_issue_type NOT IN ('GENERAL','TARGETED','CRISIS')
    OR p_face_value IS NULL OR p_face_value <= 0 OR p_face_value > 1000000000
    OR p_max_quantity IS NULL OR p_max_quantity <= 0 OR p_max_quantity > 1000000
    OR CAST(p_face_value AS UNSIGNED) * p_max_quantity > 1000000000
    OR p_yield_bps IS NULL OR p_yield_bps < 0 OR p_yield_bps > 10000
    OR p_offer_start IS NULL OR p_offer_end IS NULL OR p_maturity IS NULL
    OR p_offer_start >= p_offer_end OR p_offer_end >= p_maturity
    OR TIMESTAMPDIFF(MINUTE,p_offer_start,p_offer_end) > 43200
    OR TIMESTAMPDIFF(DAY,p_offer_end,p_maturity) > 365
    OR p_actor_id IS NULL OR p_actor_id=0 THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid bond issue draft';
 END IF;

 START TRANSACTION;
 -- Unique event keys, and issue_code, reject concurrent duplicates in InnoDB.
 SELECT issue_id,issue_code,title,purpose,issue_type,face_value,max_quantity,
        yield_basis_points,offer_start_at,offer_end_at,maturity_at,
        created_by_account_id,status
   INTO v_issue,v_code,v_title,v_purpose,v_type,v_face,v_quantity,
        v_yield,v_start,v_end,v_maturity,v_actor,v_state
   FROM government_bond_issues WHERE create_event_key=p_event_key LIMIT 1 FOR UPDATE;
 IF v_issue IS NULL THEN
   IF p_offer_end <= UTC_TIMESTAMP() OR p_maturity <= UTC_TIMESTAMP() THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond issue dates already expired';
   END IF;
   INSERT INTO government_bond_issues
     (issue_code,title,purpose,issue_type,face_value,max_quantity,yield_basis_points,
      offer_start_at,offer_end_at,maturity_at,create_event_key,created_by_account_id)
     VALUES(p_issue_code,p_title,p_purpose,p_issue_type,p_face_value,p_max_quantity,
            p_yield_bps,p_offer_start,p_offer_end,p_maturity,p_event_key,p_actor_id);
   SET v_issue=LAST_INSERT_ID();
   INSERT INTO government_bond_issue_audit
     (event_key,issue_id,action,actor_account_id,from_status,to_status)
     VALUES(p_event_key,v_issue,'CREATE',p_actor_id,NULL,'DRAFT');
   SET v_state='DRAFT';
 ELSE
   IF BINARY v_code <> BINARY p_issue_code OR v_title <> p_title
      OR v_purpose <> p_purpose OR BINARY v_type <> BINARY p_issue_type
      OR v_face <> p_face_value OR v_quantity <> p_max_quantity
      OR v_yield <> p_yield_bps OR v_start <> p_offer_start
      OR v_end <> p_offer_end OR v_maturity <> p_maturity
      OR v_actor <> p_actor_id THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond draft event collision';
   END IF;
   SET v_replayed=1;
 END IF;
 COMMIT;
 SELECT v_issue AS issue_id,v_state AS issue_status,v_replayed AS replayed;
END$$

-- Lock order: treasury first, issue second. Approval and fund commit are indivisible.
CREATE PROCEDURE gov_bond_issue_approve(
 IN p_event_key VARCHAR(64), IN p_issue_code VARCHAR(64),
 IN p_actor_id BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_balance BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_reserved BIGINT UNSIGNED;
 DECLARE v_committed BIGINT UNSIGNED;
 DECLARE v_enabled TINYINT UNSIGNED;
 DECLARE v_utilization SMALLINT UNSIGNED;
 DECLARE v_coverage SMALLINT UNSIGNED;
 DECLARE v_premium SMALLINT UNSIGNED;
 DECLARE v_grace INT UNSIGNED;
 DECLARE v_version INT UNSIGNED;
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_face INT UNSIGNED;
 DECLARE v_quantity INT UNSIGNED;
 DECLARE v_sold INT UNSIGNED;
 DECLARE v_creator BIGINT UNSIGNED;
 DECLARE v_status VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_approved_at DATETIME;
 DECLARE v_approve_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_approver BIGINT UNSIGNED;
 DECLARE v_exposure BIGINT UNSIGNED;
 DECLARE v_saved_exposure BIGINT UNSIGNED;
 DECLARE v_exposure_per_unit BIGINT UNSIGNED;
 DECLARE v_end DATETIME;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;

 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 6 AND 64
    OR LEFT(BINARY p_event_key,5) <> BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_issue_code IS NULL OR CHAR_LENGTH(p_issue_code) NOT BETWEEN 1 AND 64
    OR p_issue_code NOT REGEXP '^[A-Za-z0-9_:/.-]+$'
    OR p_actor_id IS NULL OR p_actor_id=0 THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid bond approval';
 END IF;

 START TRANSACTION;
 SELECT balance,bond_fund_reserved,bond_fund_committed,bond_fund_enabled,
        bond_fund_utilization_bps,bond_fund_coverage_bps,
        bond_fund_premium_bps,bond_fund_grace_seconds,row_version
   INTO v_balance,v_reserved,v_committed,v_enabled,v_utilization,v_coverage,
        v_premium,v_grace,v_version
   FROM treasury_accounts WHERE code='state' FOR UPDATE;
 IF v_balance IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='treasury missing';
 END IF;
 SELECT issue_id,face_value,max_quantity,sold_quantity,created_by_account_id,
        status,approve_event_key,approved_by_account_id,insured_max_exposure,offer_end_at
   INTO v_issue,v_face,v_quantity,v_sold,v_creator,v_status,v_approve_key,
        v_approver,v_saved_exposure,v_end
   FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE;
 IF v_issue IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond issue missing';
 END IF;

 IF v_approve_key IS NOT NULL THEN
   IF BINARY v_approve_key <> BINARY p_event_key OR v_approver <> p_actor_id THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='approval already assigned';
   END IF;
   SET v_replayed=1;
   SET v_exposure=v_saved_exposure;
 ELSE
   -- After GOV-05, only finance deputy/minister may approve; they may
   -- approve their own draft. Before 023, preserve GOV-02 legacy UAT rule.
   IF v_status <> 'DRAFT' OR v_sold <> 0
      OR (v_creator=p_actor_id AND NOT EXISTS (
            SELECT 1 FROM faction_members m JOIN factions f
            ON f.faction_id=m.faction_id WHERE m.account_id=p_actor_id
            AND m.faction_id=5 AND m.rank_id IN (3,4) AND f.is_enabled=1
            AND f.term_end>UNIX_TIMESTAMP()))
      OR (EXISTS (SELECT 1 FROM schema_migrations WHERE version=23)
          AND NOT EXISTS (
            SELECT 1 FROM faction_members m JOIN factions f
            ON f.faction_id=m.faction_id WHERE m.account_id=p_actor_id
            AND m.faction_id=5 AND m.rank_id IN (3,4) AND f.is_enabled=1
            AND f.term_end>UNIX_TIMESTAMP()))
      OR v_end <= UTC_TIMESTAMP() OR v_enabled <> 1
      OR v_version >= 2147483646 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond approval not allowed';
   END IF;

   -- Per-bond rounding UP guarantees worst-case payment even if split per owner.
   SET v_exposure_per_unit = (CAST(v_face AS UNSIGNED) * v_coverage + 9999) DIV 10000;
   SET v_exposure = v_exposure_per_unit * v_quantity;
   IF v_exposure = 0 OR v_exposure > 1000000000
      OR (v_committed + v_exposure) * 10000 > v_reserved * v_utilization THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='insufficient protected fund capacity';
   END IF;

   INSERT INTO bond_fund_guarantees(issue_code,max_exposure,event_key)
      VALUES(p_issue_code,v_exposure,p_event_key);
   UPDATE treasury_accounts
      SET bond_fund_committed=bond_fund_committed+v_exposure,
          row_version=row_version+1 WHERE code='state';
   SET v_committed=v_committed+v_exposure;
   SET v_version=v_version+1;
   INSERT INTO bond_fund_ledger
      (event_key,operation,reference_code,reserve_delta,commitment_delta,
       reserved_after,committed_after,treasury_row_version_after,actor_account_id)
      VALUES(p_event_key,'COMMIT',p_issue_code,0,v_exposure,
             v_reserved,v_committed,v_version,p_actor_id);
   UPDATE government_bond_issues SET
       status='APPROVED',coverage_basis_points=v_coverage,
       premium_basis_points=v_premium,grace_seconds=v_grace,
       insured_max_exposure=v_exposure,approve_event_key=p_event_key,
       approved_by_account_id=p_actor_id,approved_at=UTC_TIMESTAMP(),
       row_version=row_version+1
     WHERE issue_id=v_issue;
   INSERT INTO government_bond_issue_audit
     (event_key,issue_id,action,actor_account_id,from_status,to_status,fund_exposure_delta)
     VALUES(p_event_key,v_issue,'APPROVE',p_actor_id,'DRAFT','APPROVED',v_exposure);
   SET v_status='APPROVED';
 END IF;
 COMMIT;
 SELECT v_issue AS issue_id,v_status AS issue_status,
        v_exposure AS insured_exposure,v_committed AS fund_committed,
        v_version AS treasury_row_version,v_replayed AS replayed;
END$$

CREATE PROCEDURE gov_bond_issue_open(
 IN p_event_key VARCHAR(64), IN p_issue_code VARCHAR(64),
 IN p_actor_id BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_status VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_start DATETIME;
 DECLARE v_end DATETIME;
 DECLARE v_guarantee BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_event BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_event_actor BIGINT UNSIGNED;
 DECLARE v_event_issue BIGINT UNSIGNED;
 DECLARE v_event_action VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;

 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 6 AND 64
    OR LEFT(BINARY p_event_key,5) <> BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_issue_code IS NULL OR CHAR_LENGTH(p_issue_code) NOT BETWEEN 1 AND 64
    OR p_issue_code NOT REGEXP '^[A-Za-z0-9_:/.-]+$'
    OR p_actor_id IS NULL OR p_actor_id=0 THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid bond opening';
 END IF;
 START TRANSACTION;
 SELECT issue_id,status,offer_start_at,offer_end_at
   INTO v_issue,v_status,v_start,v_end
   FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE;
 IF v_issue IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond issue missing';
 END IF;
 SELECT event_id,issue_id,actor_account_id,action
   INTO v_event,v_event_issue,v_event_actor,v_event_action
   FROM government_bond_issue_audit WHERE event_key=p_event_key LIMIT 1;
 IF v_event IS NOT NULL THEN
   IF v_event_issue <> v_issue OR v_event_actor <> p_actor_id
      OR v_event_action <> 'OPEN' THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='opening event collision';
   END IF;
   SET v_replayed=1;
 ELSE
   IF v_status <> 'APPROVED' OR v_start > UTC_TIMESTAMP()
      OR v_end <= UTC_TIMESTAMP() THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond offering outside permitted window';
   END IF;
   SELECT max_exposure-released_exposure INTO v_guarantee
     FROM bond_fund_guarantees WHERE issue_code=p_issue_code;
   IF v_guarantee IS NULL OR v_guarantee=0 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond guarantee not funded';
   END IF;
   UPDATE government_bond_issues SET status='OFFERING',
     opened_at=UTC_TIMESTAMP(),row_version=row_version+1 WHERE issue_id=v_issue;
   INSERT INTO government_bond_issue_audit
     (event_key,issue_id,action,actor_account_id,from_status,to_status)
     VALUES(p_event_key,v_issue,'OPEN',p_actor_id,'APPROVED','OFFERING');
   SET v_status='OFFERING';
 END IF;
 COMMIT;
 SELECT v_issue AS issue_id,v_status AS issue_status,v_replayed AS replayed;
END$$

-- Cancellation only while sold_quantity=0; no holder can lose an obligation.
-- Lock order matches approval: treasury -> issue -> guarantee.
CREATE PROCEDURE gov_bond_issue_cancel(
 IN p_event_key VARCHAR(64), IN p_issue_code VARCHAR(64),
 IN p_actor_id BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_balance BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_reserved BIGINT UNSIGNED;
 DECLARE v_committed BIGINT UNSIGNED;
 DECLARE v_version INT UNSIGNED;
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_status VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_sold INT UNSIGNED;
 DECLARE v_holder_count BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_release BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_event BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_event_issue BIGINT UNSIGNED;
 DECLARE v_event_actor BIGINT UNSIGNED;
 DECLARE v_event_action VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;

 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 6 AND 64
    OR LEFT(BINARY p_event_key,5) <> BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_issue_code IS NULL OR CHAR_LENGTH(p_issue_code) NOT BETWEEN 1 AND 64
    OR p_issue_code NOT REGEXP '^[A-Za-z0-9_:/.-]+$'
    OR p_actor_id IS NULL OR p_actor_id=0 THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid bond cancellation';
 END IF;
 START TRANSACTION;
 SELECT balance,bond_fund_reserved,bond_fund_committed,row_version
   INTO v_balance,v_reserved,v_committed,v_version
   FROM treasury_accounts WHERE code='state' FOR UPDATE;
 IF v_balance IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='treasury missing';
 END IF;
 SELECT issue_id,status,sold_quantity INTO v_issue,v_status,v_sold
   FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE;
 IF v_issue IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond issue missing';
 END IF;
 SELECT event_id,issue_id,actor_account_id,action
   INTO v_event,v_event_issue,v_event_actor,v_event_action
   FROM government_bond_issue_audit WHERE event_key=p_event_key LIMIT 1;
 IF v_event IS NOT NULL THEN
   IF v_event_issue <> v_issue OR v_event_actor <> p_actor_id
      OR v_event_action <> 'CANCEL' THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='cancel event collision';
   END IF;
   SET v_replayed=1;
 ELSE
   IF v_status NOT IN ('DRAFT','APPROVED','OFFERING') OR v_sold <> 0 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='cannot cancel sold bond issue';
   END IF;
   -- Defense in depth: a stale sold_quantity must not erase ownership.
   SELECT COUNT(*) INTO v_holder_count FROM government_bond_holdings
     WHERE issue_id=v_issue;
   IF v_holder_count <> 0 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='cannot cancel issue with bond holders';
   END IF;
   IF v_status <> 'DRAFT' THEN
     SELECT max_exposure-released_exposure INTO v_release
       FROM bond_fund_guarantees WHERE issue_code=p_issue_code FOR UPDATE;
     IF v_release IS NULL OR v_release=0 OR v_committed < v_release
        OR v_version >= 2147483646 THEN
       SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid fund release';
     END IF;
     UPDATE bond_fund_guarantees SET released_exposure=max_exposure
       WHERE issue_code=p_issue_code;
     UPDATE treasury_accounts SET
       bond_fund_committed=bond_fund_committed-v_release,
       row_version=row_version+1 WHERE code='state';
     SET v_committed=v_committed-v_release;
     SET v_version=v_version+1;
     INSERT INTO bond_fund_ledger
       (event_key,operation,reference_code,reserve_delta,commitment_delta,
        reserved_after,committed_after,treasury_row_version_after,actor_account_id)
       VALUES(p_event_key,'RELEASE',p_issue_code,0,-v_release,
              v_reserved,v_committed,v_version,p_actor_id);
   END IF;
   UPDATE government_bond_issues SET status='CANCELLED',
     cancelled_at=UTC_TIMESTAMP(),row_version=row_version+1 WHERE issue_id=v_issue;
   INSERT INTO government_bond_issue_audit
     (event_key,issue_id,action,actor_account_id,from_status,to_status,fund_exposure_delta)
     VALUES(p_event_key,v_issue,'CANCEL',p_actor_id,v_status,'CANCELLED',-v_release);
   SET v_status='CANCELLED';
 END IF;
 COMMIT;
 SELECT v_issue AS issue_id,v_status AS issue_status,
        v_release AS released_exposure,v_committed AS fund_committed,
        v_version AS treasury_row_version,v_replayed AS replayed;
END$$
DELIMITER ;

-- Disallow standalone guarantee commitments after the issue-lifecycle migration.
-- GOV-01's independent API is deliberately superseded by atomic issue approval.
DELIMITER $$
DROP PROCEDURE IF EXISTS bond_fund_commit$$
CREATE PROCEDURE bond_fund_commit(
  IN p_event_key VARCHAR(64),
  IN p_issue_code VARCHAR(64),
  IN p_exposure INT,
  IN p_actor_account_id BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='standalone commitments disabled: use gov_bond_issue_approve';
END$$
DELIMITER ;

INSERT INTO schema_migrations(version) VALUES (20);
