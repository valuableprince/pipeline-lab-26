-- GOV-05: treasury governance roles, automatic year-based bond codes, auditable game UI API.
-- Requires migrations 017-022, staging only. Full finance acceptance after GOV-06.

CREATE TABLE government_bond_officials (
  account_id BIGINT UNSIGNED NOT NULL PRIMARY KEY,
  role_code VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  enabled TINYINT UNSIGNED NOT NULL DEFAULT 1,
  appointed_by BIGINT UNSIGNED NOT NULL,
  changed_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  row_version INT UNSIGNED NOT NULL DEFAULT 0,
  CONSTRAINT fk_bond_official_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT fk_bond_official_appointing FOREIGN KEY (appointed_by)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_bond_official_role CHECK
    (role_code IN ('TREASURER','CONTROLLER','DIRECTOR')),
  CONSTRAINT chk_bond_official_enabled CHECK (enabled IN (0,1))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE government_bond_role_audit (
  audit_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  actor_account_id BIGINT UNSIGNED NOT NULL,
  target_account_id BIGINT UNSIGNED NOT NULL,
  role_code VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  enabled TINYINT UNSIGNED NOT NULL,
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  UNIQUE KEY uq_bond_role_audit_event (event_key),
  KEY idx_bond_role_audit_target (target_account_id,audit_id),
  CONSTRAINT fk_bond_role_audit_actor FOREIGN KEY (actor_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT fk_bond_role_audit_target FOREIGN KEY (target_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_bond_role_audit_role CHECK
    (role_code IN ('TREASURER','CONTROLLER','DIRECTOR')),
  CONSTRAINT chk_bond_role_audit_enabled CHECK (enabled IN (0,1))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Rows are created on demand. Sequence is never reset when an issue is cancelled.
CREATE TABLE government_bond_code_sequences (
  issue_year SMALLINT UNSIGNED NOT NULL PRIMARY KEY,
  last_number INT UNSIGNED NOT NULL DEFAULT 0,
  CONSTRAINT chk_bond_sequence_year CHECK (issue_year BETWEEN 2026 AND 2100),
  CONSTRAINT chk_bond_sequence_max CHECK (last_number <= 9999)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DELIMITER $$

-- Role appointment is an internal founder-only server action.
-- Root identity currently exists in the protected Admin file, not SQL:
-- only a guarded founder Pawn call may invoke this until GOV-06 SQL RBAC isolation.
CREATE PROCEDURE gov_bond_role_set(
  IN p_event_key VARCHAR(64),IN p_actor BIGINT UNSIGNED,
  IN p_target BIGINT UNSIGNED,IN p_role VARCHAR(16),IN p_enabled TINYINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_event BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_actor BIGINT UNSIGNED;
 DECLARE v_target BIGINT UNSIGNED;
 DECLARE v_role VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_enabled TINYINT UNSIGNED;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
   OR LEFT(BINARY p_event_key,5)<>BINARY 'bond:'
   OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
   OR p_actor IS NULL OR p_actor=0 OR p_target IS NULL OR p_target=0
   OR p_role NOT IN ('TREASURER','CONTROLLER','DIRECTOR')
   OR p_enabled NOT IN (0,1) THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid government appointment';
 END IF;
 START TRANSACTION;
 -- Lock an existing target account, and serialize target grant/revocation.
 SELECT account_id INTO v_target FROM accounts WHERE account_id=p_target FOR UPDATE;
 IF v_target IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='government appointee missing';
 END IF;
 SELECT audit_id,actor_account_id,target_account_id,role_code,enabled
    INTO v_event,v_actor,v_target,v_role,v_enabled
    FROM government_bond_role_audit WHERE event_key=p_event_key LIMIT 1;
 IF v_event IS NOT NULL THEN
   IF v_actor<>p_actor OR v_target<>p_target OR BINARY v_role<>BINARY p_role
      OR v_enabled<>p_enabled THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='government role event collision';
   END IF;
   SET v_replayed=1;
 ELSE
   INSERT INTO government_bond_officials
     (account_id,role_code,enabled,appointed_by)
     VALUES(p_target,p_role,p_enabled,p_actor)
     ON DUPLICATE KEY UPDATE
       role_code=VALUES(role_code),enabled=VALUES(enabled),
       appointed_by=VALUES(appointed_by),
       changed_at=UTC_TIMESTAMP(),row_version=row_version+1;
   INSERT INTO government_bond_role_audit
     (event_key,actor_account_id,target_account_id,role_code,enabled)
     VALUES(p_event_key,p_actor,p_target,p_role,p_enabled);
 END IF;
 COMMIT;
 SELECT p_target AS account_id,p_role AS role_code,p_enabled AS enabled,
        v_replayed AS replayed;
END$$

-- Single InnoDB transaction: role check, per-year sequence reservation,
-- issue creation and issue-audit insertion. Never pre-generate IDs in Pawn.
CREATE PROCEDURE gov_bond_issue_create_auto(
  IN p_event_key VARCHAR(64),IN p_actor BIGINT UNSIGNED,
  IN p_title VARCHAR(128),IN p_purpose VARCHAR(512),
  IN p_type VARCHAR(16),IN p_face INT,IN p_quantity INT,IN p_yield_bps INT,
  IN p_offer_days INT,IN p_maturity_days INT
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_role VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL;
 DECLARE v_year SMALLINT UNSIGNED;
 DECLARE v_number INT UNSIGNED;
 DECLARE v_code VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE v_title VARCHAR(128);
 DECLARE v_purpose VARCHAR(512);
 DECLARE v_type VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_face INT UNSIGNED;
 DECLARE v_qty INT UNSIGNED;
 DECLARE v_yield SMALLINT UNSIGNED;
 DECLARE v_start DATETIME;
 DECLARE v_end DATETIME;
 DECLARE v_maturity DATETIME;
 DECLARE v_creator BIGINT UNSIGNED;
 DECLARE v_start_new DATETIME;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 7 AND 64
    OR LEFT(BINARY p_event_key,5)<>BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_actor IS NULL OR p_actor=0
    OR p_title IS NULL OR CHAR_LENGTH(TRIM(p_title)) NOT BETWEEN 3 AND 128
    OR p_purpose IS NULL OR CHAR_LENGTH(TRIM(p_purpose)) NOT BETWEEN 3 AND 512
    OR p_type NOT IN ('GENERAL','TARGETED','CRISIS')
    OR p_face IS NULL OR p_face<1 OR p_face>1000000000
    OR p_quantity IS NULL OR p_quantity<1 OR p_quantity>1000000
    OR CAST(p_face AS UNSIGNED)*p_quantity>1000000000
    OR p_yield_bps IS NULL OR p_yield_bps<0 OR p_yield_bps>10000
    OR p_offer_days IS NULL OR p_offer_days NOT BETWEEN 1 AND 30
    OR p_maturity_days IS NULL OR p_maturity_days NOT BETWEEN 1 AND 365 THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid automated government issue';
 END IF;
 START TRANSACTION;
 SELECT role_code INTO v_role FROM government_bond_officials
    WHERE account_id=p_actor AND enabled=1 FOR UPDATE;
 IF v_role NOT IN ('TREASURER','DIRECTOR') OR v_role IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='treasury issue creator unauthorized';
 END IF;
 SELECT issue_id,issue_code,title,purpose,issue_type,face_value,max_quantity,
        yield_basis_points,offer_start_at,offer_end_at,maturity_at,created_by_account_id
   INTO v_issue,v_code,v_title,v_purpose,v_type,v_face,v_qty,
        v_yield,v_start,v_end,v_maturity,v_creator
   FROM government_bond_issues WHERE create_event_key=p_event_key LIMIT 1 FOR UPDATE;
 IF v_issue IS NOT NULL THEN
   IF v_title<>p_title OR v_purpose<>p_purpose OR
      BINARY v_type<>BINARY p_type OR v_face<>p_face OR
      v_qty<>p_quantity OR v_yield<>p_yield_bps OR v_creator<>p_actor
      OR TIMESTAMPDIFF(DAY,v_start,v_end)<>p_offer_days
      OR TIMESTAMPDIFF(DAY,v_end,v_maturity)<>p_maturity_days THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='automated issue event collision';
   END IF;
   SET v_replayed=1;
 ELSE
   SET v_start_new=UTC_TIMESTAMP();
   SET v_year=YEAR(v_start_new);
   IF v_year NOT BETWEEN 2026 AND 2100 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond code year unsupported';
   END IF;
   INSERT INTO government_bond_code_sequences(issue_year,last_number)
     VALUES(v_year,0) ON DUPLICATE KEY UPDATE issue_year=VALUES(issue_year);
   SELECT last_number INTO v_number
     FROM government_bond_code_sequences WHERE issue_year=v_year FOR UPDATE;
   IF v_number>=9999 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='annual bond sequence exhausted';
   END IF;
   SET v_number=v_number+1;
   UPDATE government_bond_code_sequences SET last_number=v_number
     WHERE issue_year=v_year;
   SET v_code=CONCAT('GOV-',v_year,'-',LPAD(v_number,4,'0'));
   INSERT INTO government_bond_issues
     (issue_code,title,purpose,issue_type,face_value,max_quantity,
      yield_basis_points,offer_start_at,offer_end_at,maturity_at,
      create_event_key,created_by_account_id)
     VALUES(v_code,p_title,p_purpose,p_type,p_face,p_quantity,p_yield_bps,
       v_start_new,DATE_ADD(v_start_new,INTERVAL p_offer_days DAY),
       DATE_ADD(DATE_ADD(v_start_new,INTERVAL p_offer_days DAY),
                INTERVAL p_maturity_days DAY),
       p_event_key,p_actor);
   SET v_issue=LAST_INSERT_ID();
   INSERT INTO government_bond_issue_audit
     (event_key,issue_id,action,actor_account_id,from_status,to_status)
     VALUES(p_event_key,v_issue,'CREATE',p_actor,NULL,'DRAFT');
 END IF;
 COMMIT;
 SELECT v_issue AS issue_id,v_code AS issue_code,
        'DRAFT' AS issue_status,v_replayed AS replayed;
END$$

-- Authorization wrappers deliberately recheck persisted roles at CALL time.
-- Underlying GOV-02 routines enforce two-person signature and reserve coverage.
CREATE PROCEDURE gov_bond_official_approve(
  IN p_event_key VARCHAR(64),IN p_issue_code VARCHAR(64),
  IN p_actor BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_role VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL;
 SELECT role_code INTO v_role FROM government_bond_officials
    WHERE account_id=p_actor AND enabled=1;
 IF v_role IS NULL OR v_role NOT IN ('CONTROLLER','DIRECTOR') THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond approver role required';
 END IF;
 CALL gov_bond_issue_approve(p_event_key,p_issue_code,p_actor);
END$$
CREATE PROCEDURE gov_bond_official_open(
  IN p_event_key VARCHAR(64),IN p_issue_code VARCHAR(64),
  IN p_actor BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_role VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL;
 SELECT role_code INTO v_role FROM government_bond_officials
    WHERE account_id=p_actor AND enabled=1;
 IF v_role IS NULL OR v_role NOT IN ('TREASURER','DIRECTOR') THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='treasurer role required for offering';
 END IF;
 CALL gov_bond_issue_open(p_event_key,p_issue_code,p_actor);
END$$
CREATE PROCEDURE gov_bond_official_cancel(
  IN p_event_key VARCHAR(64),IN p_issue_code VARCHAR(64),
  IN p_actor BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_role VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL;
 SELECT role_code INTO v_role FROM government_bond_officials
    WHERE account_id=p_actor AND enabled=1;
 IF v_role IS NULL OR v_role NOT IN ('CONTROLLER','DIRECTOR') THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond controller role required for cancellation';
 END IF;
 CALL gov_bond_issue_cancel(p_event_key,p_issue_code,p_actor);
END$$
DELIMITER ;

INSERT INTO schema_migrations(version) VALUES (23);
