-- GOV-05.1 / migration 025, apply ONLY after 024 to isolated UAT.
-- Reissued financial routines must use the designated permanent DBA definer.

ALTER TABLE government_bond_issues
  DROP CONSTRAINT chk_government_bond_status,
  ADD CONSTRAINT chk_government_bond_status CHECK
    (status IN ('DRAFT','RETURNED','APPROVED','OFFERING','CLOSED','CANCELLED','REJECTED')),
  ADD COLUMN returned_reason VARCHAR(255) NULL,
  ADD COLUMN returned_by_account_id BIGINT UNSIGNED NULL,
  ADD COLUMN returned_at DATETIME NULL,
  ADD COLUMN revision_count INT UNSIGNED NOT NULL DEFAULT 0,
  ADD CONSTRAINT chk_government_bond_returned CHECK (
    (status='RETURNED' AND returned_reason IS NOT NULL
      AND CHAR_LENGTH(TRIM(returned_reason)) BETWEEN 5 AND 255
      AND returned_by_account_id IS NOT NULL AND returned_at IS NOT NULL)
    OR (status<>'RETURNED' AND returned_reason IS NULL
      AND returned_by_account_id IS NULL AND returned_at IS NULL)),
  ADD CONSTRAINT chk_government_bond_revision_count CHECK (revision_count<=1000),
  ADD CONSTRAINT fk_government_bond_returner FOREIGN KEY (returned_by_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT;

ALTER TABLE government_bond_issue_audit
  DROP CONSTRAINT chk_bond_issue_audit_action,
  ADD CONSTRAINT chk_bond_issue_audit_action CHECK
    (action IN ('CREATE','APPROVE','OPEN','CANCEL','REJECT','RETURN','RESUBMIT'));

CREATE TABLE government_bond_revision_reviews (
  review_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  issue_id BIGINT UNSIGNED NOT NULL,
  event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  reviewer_account_id BIGINT UNSIGNED NOT NULL,
  revision_number INT UNSIGNED NOT NULL,
  reason VARCHAR(255) NOT NULL,
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  UNIQUE KEY uq_government_bond_revision_event (event_key),
  UNIQUE KEY uq_government_bond_revision_issue (issue_id,revision_number),
  CONSTRAINT fk_gov_revision_issue FOREIGN KEY (issue_id)
    REFERENCES government_bond_issues(issue_id) ON DELETE RESTRICT,
  CONSTRAINT fk_gov_revision_reviewer FOREIGN KEY (reviewer_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_gov_revision_reason CHECK (CHAR_LENGTH(TRIM(reason)) BETWEEN 5 AND 255)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DELIMITER $$
CREATE TRIGGER gov_bond_revision_reviews_upd BEFORE UPDATE ON government_bond_revision_reviews
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='immutable bond revision reviews';
END$$
CREATE TRIGGER gov_bond_revision_reviews_del BEFORE DELETE ON government_bond_revision_reviews
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='immutable bond revision reviews';
END$$

DROP TRIGGER IF EXISTS government_bond_terms_immutable$$
CREATE TRIGGER government_bond_terms_immutable
BEFORE UPDATE ON government_bond_issues FOR EACH ROW
BEGIN
  IF OLD.status NOT IN ('DRAFT','RETURNED') AND (
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
       OR ((OLD.offer_start_at <> NEW.offer_start_at
         OR OLD.offer_end_at <> NEW.offer_end_at
         OR OLD.maturity_at <> NEW.maturity_at)
         AND NOT (OLD.status='APPROVED' AND NEW.status='OFFERING'
           AND BINARY OLD.issue_code REGEXP '^GOV-[0-9]{6}
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
           AND NEW.opened_at IS NOT NULL AND NEW.offer_start_at=NEW.opened_at
           AND TIMESTAMPDIFF(DAY,OLD.offer_start_at,OLD.offer_end_at)
               =TIMESTAMPDIFF(DAY,NEW.offer_start_at,NEW.offer_end_at)
           AND TIMESTAMPDIFF(DAY,OLD.offer_end_at,OLD.maturity_at)
               =TIMESTAMPDIFF(DAY,NEW.offer_end_at,NEW.maturity_at)))
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

-- GOV-02-compatible procedures rebuilt after GOV-06 assurance.
DROP PROCEDURE IF EXISTS gov_bond_issue_approve$$
CREATE PROCEDURE gov_bond_issue_approve(
 IN p_event_key VARCHAR(64), IN p_issue_code VARCHAR(64),
 IN p_actor_id BIGINT UNSIGNED
)
SQL SECURITY DEFINER
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
      OR (v_end <= UTC_TIMESTAMP()
          AND BINARY p_issue_code NOT REGEXP '^GOV-[0-9]{6}
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
END$$)
      OR v_enabled <> 1
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

DROP PROCEDURE IF EXISTS gov_bond_issue_open$$
CREATE PROCEDURE gov_bond_issue_open(
 IN p_event_key VARCHAR(64), IN p_issue_code VARCHAR(64),
 IN p_actor_id BIGINT UNSIGNED
)
SQL SECURITY DEFINER
BEGIN
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_status VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_start DATETIME;
 DECLARE v_end DATETIME;
 DECLARE v_maturity DATETIME;
 DECLARE v_offer_days INT;
 DECLARE v_maturity_days INT;
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
 SELECT issue_id,status,offer_start_at,offer_end_at,maturity_at
   INTO v_issue,v_status,v_start,v_end,v_maturity
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
   IF v_status <> 'APPROVED' OR
       (BINARY p_issue_code NOT REGEXP '^GOV-[0-9]{6}
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond offering outside permitted window';
   END IF;
   SELECT max_exposure-released_exposure INTO v_guarantee
     FROM bond_fund_guarantees WHERE issue_code=p_issue_code;
   IF v_guarantee IS NULL OR v_guarantee=0 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond guarantee not funded';
   END IF;
   IF BINARY p_issue_code REGEXP '^GOV-[0-9]{6}
   INSERT INTO government_bond_issue_audit
     (event_key,issue_id,action,actor_account_id,from_status,to_status)
     VALUES(p_event_key,v_issue,'OPEN',p_actor_id,'APPROVED','OFFERING');
   SET v_status='OFFERING';
 END IF;
 COMMIT;
 SELECT v_issue AS issue_id,v_status AS issue_status,v_replayed AS replayed;
END$$ AND
        (v_start>UTC_TIMESTAMP() OR v_end<=UTC_TIMESTAMP())) THEN
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
END$$ THEN
     SET v_offer_days=TIMESTAMPDIFF(DAY,v_start,v_end);
     SET v_maturity_days=TIMESTAMPDIFF(DAY,v_end,v_maturity);
     IF v_offer_days NOT BETWEEN 1 AND 30
       OR v_maturity_days NOT BETWEEN 1 AND 365 THEN
       SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid official bond durations';
     END IF;
     UPDATE government_bond_issues SET status='OFFERING',
       opened_at=UTC_TIMESTAMP(),offer_start_at=UTC_TIMESTAMP(),
       offer_end_at=DATE_ADD(UTC_TIMESTAMP(),INTERVAL v_offer_days DAY),
       maturity_at=DATE_ADD(DATE_ADD(UTC_TIMESTAMP(),INTERVAL v_offer_days DAY),
         INTERVAL v_maturity_days DAY),row_version=row_version+1 WHERE issue_id=v_issue;
   ELSE
     UPDATE government_bond_issues SET status='OFFERING',
       opened_at=UTC_TIMESTAMP(),row_version=row_version+1 WHERE issue_id=v_issue;
   END IF;
   INSERT INTO government_bond_issue_audit
     (event_key,issue_id,action,actor_account_id,from_status,to_status)
     VALUES(p_event_key,v_issue,'OPEN',p_actor_id,'APPROVED','OFFERING');
   SET v_status='OFFERING';
 END IF;
 COMMIT;
 SELECT v_issue AS issue_id,v_status AS issue_status,v_replayed AS replayed;
END$$ AND
        (v_start>UTC_TIMESTAMP() OR v_end<=UTC_TIMESTAMP())) THEN
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

-- RETURN is not a rejection: it preserves the GOV code and permits
-- only its creator to resubmit. The review is append-only evidence.
CREATE PROCEDURE gov_bond_official_return(
 IN p_event_key VARCHAR(64),IN p_issue_code VARCHAR(64),
 IN p_actor BIGINT UNSIGNED,IN p_reason VARCHAR(255)
)
SQL SECURITY DEFINER
BEGIN
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_creator BIGINT UNSIGNED;
 DECLARE v_status VARCHAR(16);
 DECLARE v_rank TINYINT UNSIGNED DEFAULT NULL;
 DECLARE v_revision INT UNSIGNED;
 DECLARE v_event BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_event_actor BIGINT UNSIGNED;
 DECLARE v_event_issue BIGINT UNSIGNED;
 DECLARE v_event_action VARCHAR(16);
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
   OR LEFT(BINARY p_event_key,5)<>BINARY 'bond:'
   OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
   OR p_issue_code IS NULL OR p_issue_code NOT REGEXP '^GOV-[0-9]{6}$'
   OR p_actor IS NULL OR p_actor=0 OR p_reason IS NULL
   OR CHAR_LENGTH(TRIM(p_reason)) NOT BETWEEN 5 AND 160
   OR p_reason REGEXP '[[:cntrl:]]' OR p_reason LIKE '%{%' OR p_reason LIKE '%}%' THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid bond return';
 END IF;
 START TRANSACTION;
 SELECT issue_id,created_by_account_id,status,revision_count
   INTO v_issue,v_creator,v_status,v_revision
   FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE;
 IF v_issue IS NULL THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond missing'; END IF;
 SELECT event_id,issue_id,actor_account_id,action
   INTO v_event,v_event_issue,v_event_actor,v_event_action
   FROM government_bond_issue_audit WHERE event_key=p_event_key LIMIT 1;
 IF v_event IS NOT NULL THEN
   IF v_event_issue<>v_issue OR v_event_actor<>p_actor OR v_event_action<>'RETURN'
     OR NOT EXISTS(SELECT 1 FROM government_bond_revision_reviews
          WHERE event_key=p_event_key AND BINARY reason=BINARY p_reason) THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='return replay collision';
   END IF;
   SET v_replayed=1;
 ELSE
   SELECT m.rank_id INTO v_rank FROM faction_members m JOIN factions f
     ON f.faction_id=m.faction_id WHERE m.account_id=p_actor AND m.faction_id=5
     AND f.is_enabled=1 AND f.term_end>UNIX_TIMESTAMP();
   IF v_rank IS NULL OR v_rank NOT IN (3,4) OR v_status<>'DRAFT'
      OR v_revision>=1000 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='return requires valid draft and official';
   END IF;
   UPDATE government_bond_issues
     SET status='RETURNED',returned_reason=p_reason,
       returned_by_account_id=p_actor,returned_at=UTC_TIMESTAMP(),
       revision_count=revision_count+1,row_version=row_version+1
     WHERE issue_id=v_issue;
   INSERT INTO government_bond_revision_reviews
     (issue_id,event_key,reviewer_account_id,revision_number,reason)
     VALUES(v_issue,p_event_key,p_actor,v_revision+1,p_reason);
   INSERT INTO government_bond_issue_audit
     (event_key,issue_id,action,actor_account_id,from_status,to_status)
     VALUES(p_event_key,v_issue,'RETURN',p_actor,'DRAFT','RETURNED');
   SET v_status='RETURNED';
 END IF;
 COMMIT;
 SELECT v_issue AS issue_id,v_status AS issue_status,
    v_creator AS creator_account_id,p_reason AS return_reason,
    v_replayed AS replayed;
END$$

CREATE PROCEDURE gov_bond_issue_resubmit(
 IN p_event_key VARCHAR(64),IN p_issue_code VARCHAR(64),
 IN p_actor BIGINT UNSIGNED,IN p_title VARCHAR(128),
 IN p_purpose VARCHAR(512),IN p_type VARCHAR(16),
 IN p_face INT,IN p_quantity INT,IN p_yield_bps INT,
 IN p_offer_days INT,IN p_maturity_days INT
)
SQL SECURITY DEFINER
BEGIN
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_creator BIGINT UNSIGNED;
 DECLARE v_status VARCHAR(16);
 DECLARE v_rank TINYINT UNSIGNED DEFAULT NULL;
 DECLARE v_event BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_event_actor BIGINT UNSIGNED;
 DECLARE v_event_issue BIGINT UNSIGNED;
 DECLARE v_event_action VARCHAR(16);
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE v_start DATETIME;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
   OR LEFT(BINARY p_event_key,5)<>BINARY 'bond:'
   OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
   OR p_issue_code IS NULL OR p_issue_code NOT REGEXP '^GOV-[0-9]{6}$'
   OR p_actor IS NULL OR p_actor=0
   OR p_title IS NULL OR CHAR_LENGTH(TRIM(p_title)) NOT BETWEEN 3 AND 128
   OR p_purpose IS NULL OR CHAR_LENGTH(TRIM(p_purpose)) NOT BETWEEN 3 AND 192
   OR p_type NOT IN ('GENERAL','TARGETED','CRISIS')
   OR p_face IS NULL OR p_face NOT BETWEEN 1 AND 1000000000
   OR p_quantity IS NULL OR p_quantity NOT BETWEEN 1 AND 1000000
   OR CAST(p_face AS UNSIGNED)*p_quantity > 1000000000
   OR p_yield_bps IS NULL OR p_yield_bps NOT BETWEEN 0 AND 10000
   OR p_offer_days IS NULL OR p_offer_days NOT BETWEEN 1 AND 30
   OR p_maturity_days IS NULL OR p_maturity_days NOT BETWEEN 1 AND 365 THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid bond resubmission';
 END IF;
 START TRANSACTION;
 SELECT issue_id,created_by_account_id,status
   INTO v_issue,v_creator,v_status FROM government_bond_issues
   WHERE issue_code=p_issue_code FOR UPDATE;
 IF v_issue IS NULL THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond missing'; END IF;
 SELECT event_id,issue_id,actor_account_id,action
   INTO v_event,v_event_issue,v_event_actor,v_event_action
   FROM government_bond_issue_audit WHERE event_key=p_event_key LIMIT 1;
 IF v_event IS NOT NULL THEN
   IF v_event_issue<>v_issue OR v_event_actor<>p_actor OR v_event_action<>'RESUBMIT'
     OR BINARY (SELECT title FROM government_bond_issues WHERE issue_id=v_issue)<>BINARY p_title
     OR BINARY (SELECT purpose FROM government_bond_issues WHERE issue_id=v_issue)<>BINARY p_purpose
     OR (SELECT face_value FROM government_bond_issues WHERE issue_id=v_issue)<>p_face
     OR (SELECT max_quantity FROM government_bond_issues WHERE issue_id=v_issue)<>p_quantity
     OR (SELECT yield_basis_points FROM government_bond_issues WHERE issue_id=v_issue)<>p_yield_bps THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='resubmit replay collision';
   END IF;
   SET v_replayed=1;
 ELSE
   SELECT m.rank_id INTO v_rank FROM faction_members m JOIN factions f
     ON f.faction_id=m.faction_id WHERE m.account_id=p_actor AND m.faction_id=5
     AND f.is_enabled=1 AND f.term_end>UNIX_TIMESTAMP();
   IF v_creator<>p_actor OR v_status<>'RETURNED'
     OR v_rank IS NULL OR v_rank NOT IN (2,3,4) THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='resubmit requires original author and returned state';
   END IF;
   SET v_start=UTC_TIMESTAMP();
   UPDATE government_bond_issues SET status='DRAFT',
     title=p_title,purpose=p_purpose,issue_type=p_type,
     face_value=p_face,max_quantity=p_quantity,yield_basis_points=p_yield_bps,
     offer_start_at=v_start,
     offer_end_at=DATE_ADD(v_start,INTERVAL p_offer_days DAY),
     maturity_at=DATE_ADD(DATE_ADD(v_start,INTERVAL p_offer_days DAY),
                          INTERVAL p_maturity_days DAY),
     returned_reason=NULL,returned_by_account_id=NULL,returned_at=NULL,
     row_version=row_version+1 WHERE issue_id=v_issue;
   INSERT INTO government_bond_issue_audit
     (event_key,issue_id,action,actor_account_id,from_status,to_status)
     VALUES(p_event_key,v_issue,'RESUBMIT',p_actor,'RETURNED','DRAFT');
   SET v_status='DRAFT';
 END IF;
 COMMIT;
 SELECT v_issue AS issue_id,p_issue_code AS issue_code,
    v_status AS issue_status,v_replayed AS replayed;
END$$

DELIMITER ;
INSERT INTO schema_migrations(version) VALUES (25);
