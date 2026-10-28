-- GOV-05: treasury governance roles, globally sequential bond codes, auditable game UI API.
-- Requires migrations 017-022, staging only. Full finance acceptance after GOV-06.

-- Government organization #5 uses the EXISTING faction/roster infrastructure.
-- Rank 1 trainee, 2 specialist, 3 deputy, 4 minister (stored exactly).
INSERT INTO factions(faction_id,is_enabled,term_start,term_end)
 VALUES(5,1,0,0) ON DUPLICATE KEY UPDATE is_enabled=1;

-- One persistent, globally increasing counter, never reset across years.
CREATE TABLE government_bond_code_sequences (
  sequence_id TINYINT UNSIGNED NOT NULL PRIMARY KEY,
  last_number INT UNSIGNED NOT NULL DEFAULT 0,
  CONSTRAINT chk_bond_sequence_singleton CHECK (sequence_id = 1),
  CONSTRAINT chk_bond_sequence_max CHECK (last_number <= 999999)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Seed from the highest existing six-digit GOV number without changing old codes.
INSERT INTO government_bond_code_sequences(sequence_id,last_number)
 SELECT 1, COALESCE(MAX(CAST(SUBSTRING(issue_code,5) AS UNSIGNED)),0)
 FROM government_bond_issues
 WHERE BINARY issue_code REGEXP '^GOV-[0-9]{6}$';

-- The 5% income-tax reserve rate already exists since GOV-01.
ALTER TABLE bond_fund_ledger
  DROP CONSTRAINT chk_bond_fund_event_operation,
  ADD CONSTRAINT chk_bond_fund_event_operation CHECK
    (operation IN ('CAPITALIZE','COMMIT','RELEASE','PREMIUM',
                   'INSURANCE_ALLOCATE','TAX_EARMARK'));

CREATE TABLE government_bond_tax_earmarks (
  tax_ledger_id BIGINT UNSIGNED NOT NULL PRIMARY KEY,
  fund_event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  tax_event_key VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  receipt_amount BIGINT UNSIGNED NOT NULL,
  contribution BIGINT UNSIGNED NOT NULL,
  applied_rate_bps SMALLINT UNSIGNED NOT NULL,
  treasury_row_version_after INT UNSIGNED NOT NULL,
  created_at DATETIME NOT NULL DEFAULT (UTC_TIMESTAMP()),
  UNIQUE KEY uq_gov_tax_earmark_event (fund_event_key),
  UNIQUE KEY uq_gov_tax_earmark_tax_event (tax_event_key),
  CONSTRAINT fk_gov_tax_source FOREIGN KEY (tax_ledger_id)
    REFERENCES treasury_ledger(ledger_id) ON DELETE RESTRICT,
  CONSTRAINT chk_gov_tax_contribution CHECK (contribution<=receipt_amount)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DELIMITER $$
-- Codes are immutable even while a bond is in DRAFT.
CREATE TRIGGER government_bond_code_immutable
BEFORE UPDATE ON government_bond_issues FOR EACH ROW
BEGIN
 IF BINARY OLD.issue_code<>BINARY NEW.issue_code THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='government bond issue code immutable';
 END IF;
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
 DECLARE v_rank TINYINT UNSIGNED DEFAULT NULL;
 DECLARE v_number INT UNSIGNED DEFAULT NULL;
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
 SELECT m.rank_id INTO v_rank
   FROM faction_members m JOIN factions f ON f.faction_id=m.faction_id
   WHERE m.account_id=p_actor AND m.faction_id=5 AND f.is_enabled=1
     AND f.term_end>UNIX_TIMESTAMP() FOR UPDATE;
 IF v_rank IS NULL OR v_rank NOT IN (2,3,4) THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='finance ministry specialist rank required';
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
   SELECT last_number INTO v_number
     FROM government_bond_code_sequences WHERE sequence_id=1 FOR UPDATE;
   IF v_number IS NULL OR v_number>=999999 THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='global bond sequence exhausted';
   END IF;
   SET v_number=v_number+1;
   UPDATE government_bond_code_sequences SET last_number=v_number
     WHERE sequence_id=1;
   SET v_code=CONCAT('GOV-',LPAD(v_number,6,'0'));
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

-- Finance actions check REAL faction rank on each call, not stored SQL roles.
-- A deputy or minister may approve their OWN draft (GOV-02 permits rank 3/4).
CREATE PROCEDURE gov_bond_official_approve(
 IN p_event_key VARCHAR(64),IN p_issue_code VARCHAR(64),IN p_actor BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_rank TINYINT UNSIGNED DEFAULT NULL;
 SELECT m.rank_id INTO v_rank FROM faction_members m JOIN factions f
   ON f.faction_id=m.faction_id
   WHERE m.account_id=p_actor AND m.faction_id=5 AND f.is_enabled=1
     AND f.term_end>UNIX_TIMESTAMP();
 IF v_rank IS NULL OR v_rank NOT IN (3,4) THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='finance deputy or minister required';
 END IF;
 CALL gov_bond_issue_approve(p_event_key,p_issue_code,p_actor);
END$$

CREATE PROCEDURE gov_bond_official_open(
 IN p_event_key VARCHAR(64),IN p_issue_code VARCHAR(64),IN p_actor BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_rank TINYINT UNSIGNED DEFAULT NULL;
 SELECT m.rank_id INTO v_rank FROM faction_members m JOIN factions f
   ON f.faction_id=m.faction_id
   WHERE m.account_id=p_actor AND m.faction_id=5 AND f.is_enabled=1
     AND f.term_end>UNIX_TIMESTAMP();
 IF v_rank IS NULL OR v_rank NOT IN (3,4) THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='finance deputy or minister required';
 END IF;
 CALL gov_bond_issue_open(p_event_key,p_issue_code,p_actor);
END$$

CREATE PROCEDURE gov_bond_official_cancel(
 IN p_event_key VARCHAR(64),IN p_issue_code VARCHAR(64),IN p_actor BIGINT UNSIGNED
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_rank TINYINT UNSIGNED DEFAULT NULL;
 SELECT m.rank_id INTO v_rank FROM faction_members m JOIN factions f
   ON f.faction_id=m.faction_id
   WHERE m.account_id=p_actor AND m.faction_id=5 AND f.is_enabled=1
     AND f.term_end>UNIX_TIMESTAMP();
 IF v_rank IS NULL OR v_rank NOT IN (3,4) THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='finance deputy or minister required';
 END IF;
 CALL gov_bond_issue_cancel(p_event_key,p_issue_code,p_actor);
END$$

-- Only a DRAFT can be rejected, by a deputy or minister with a reason.
-- Lock order matches approval: treasury then issue; no money is moved.
CREATE PROCEDURE gov_bond_official_reject(
 IN p_event_key VARCHAR(64),IN p_issue_code VARCHAR(64),
 IN p_actor BIGINT UNSIGNED,IN p_reason VARCHAR(255)
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_treasury BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_issue BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_creator BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_status VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL;
 DECLARE v_reason VARCHAR(255) DEFAULT NULL;
 DECLARE v_rank TINYINT UNSIGNED DEFAULT NULL;
 DECLARE v_event BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_event_issue BIGINT UNSIGNED;
 DECLARE v_event_actor BIGINT UNSIGNED;
 DECLARE v_event_action VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 6 AND 64
    OR LEFT(BINARY p_event_key,5)<>BINARY 'bond:'
    OR p_event_key NOT REGEXP '^bond:[A-Za-z0-9_:/.-]+$'
    OR p_issue_code IS NULL OR p_issue_code NOT REGEXP '^GOV-[0-9]{6}$'
    OR p_actor IS NULL OR p_actor=0
    OR p_reason IS NULL OR CHAR_LENGTH(TRIM(p_reason)) NOT BETWEEN 5 AND 255
    OR p_reason REGEXP '[[:cntrl:]]'
    OR p_reason LIKE '%{%' OR p_reason LIKE '%}%' THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid bond rejection reason or event';
 END IF;
 START TRANSACTION;
 SELECT balance INTO v_treasury FROM treasury_accounts
   WHERE code='state' FOR UPDATE;
 IF v_treasury IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='treasury not available';
 END IF;
 SELECT issue_id,created_by_account_id,status,rejection_reason
   INTO v_issue,v_creator,v_status,v_reason
   FROM government_bond_issues WHERE issue_code=p_issue_code FOR UPDATE;
 IF v_issue IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond issue not found';
 END IF;
 SELECT event_id,issue_id,actor_account_id,action
   INTO v_event,v_event_issue,v_event_actor,v_event_action
   FROM government_bond_issue_audit WHERE event_key=p_event_key LIMIT 1;
 IF v_event IS NOT NULL THEN
   IF v_event_issue<>v_issue OR v_event_actor<>p_actor
      OR BINARY v_event_action<>BINARY 'REJECT'
      OR v_status<>'REJECTED' OR BINARY v_reason<>BINARY p_reason THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='bond rejection event collision';
   END IF;
   SET v_replayed=1;
 ELSE
   SELECT m.rank_id INTO v_rank
     FROM faction_members m JOIN factions f ON f.faction_id=m.faction_id
     WHERE m.account_id=p_actor AND m.faction_id=5 AND f.is_enabled=1
       AND f.term_end>UNIX_TIMESTAMP();
   IF v_rank IS NULL OR v_rank NOT IN (3,4) THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='finance deputy or minister required';
   END IF;
   IF v_status<>'DRAFT' THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='only draft bond issues may be rejected';
   END IF;
   UPDATE government_bond_issues
     SET status='REJECTED',rejected_at=UTC_TIMESTAMP(),
         rejected_by_account_id=p_actor,rejection_reason=p_reason,
         row_version=row_version+1 WHERE issue_id=v_issue;
   INSERT INTO government_bond_issue_audit
     (event_key,issue_id,action,actor_account_id,from_status,to_status,fund_exposure_delta)
     VALUES(p_event_key,v_issue,'REJECT',p_actor,'DRAFT','REJECTED',0);
   SET v_status='REJECTED';
   SET v_reason=p_reason;
 END IF;
 COMMIT;
 SELECT v_issue AS issue_id,v_status AS issue_status,
        v_creator AS creator_account_id,v_reason AS rejection_reason,
        v_replayed AS replayed;
END$$

-- Each tax credit is a real positive treasury ledger row, and can fund F ONCE.
CREATE PROCEDURE gov_bond_tax_earmark(
 IN p_event_key VARCHAR(64), IN p_tax_event_key VARCHAR(64)
)
SQL SECURITY INVOKER
BEGIN
 DECLARE v_balance BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_fund BIGINT UNSIGNED;
 DECLARE v_payout BIGINT UNSIGNED;
 DECLARE v_committed BIGINT UNSIGNED;
 DECLARE v_min BIGINT UNSIGNED;
 DECLARE v_version INT UNSIGNED;
 DECLARE v_enabled TINYINT UNSIGNED;
 DECLARE v_rate SMALLINT UNSIGNED;
 DECLARE v_tax_id BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_tax_amount BIGINT;
 DECLARE v_source VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_existing BIGINT UNSIGNED DEFAULT NULL;
 DECLARE v_existing_event VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin;
 DECLARE v_amount BIGINT UNSIGNED DEFAULT 0;
 DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;
 DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
 IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 10 AND 64
    OR LEFT(BINARY p_event_key,9)<>BINARY 'fund:tax:'
    OR p_event_key NOT REGEXP '^fund:tax:[0-9]+$'
    OR p_tax_event_key IS NULL OR CHAR_LENGTH(p_tax_event_key) NOT BETWEEN 1 AND 64 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid tax earmark event';
 END IF;
 START TRANSACTION;
 SELECT balance,bond_fund_reserved,bond_payout_reserved,
        bond_fund_committed,minimum_reserve,row_version,
        bond_fund_enabled,bond_fund_tax_allocation_bps
   INTO v_balance,v_fund,v_payout,v_committed,v_min,v_version,v_enabled,v_rate
   FROM treasury_accounts WHERE code='state' FOR UPDATE;
 IF v_balance IS NULL THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='tax treasury missing';
 END IF;
 SELECT ledger_id,amount_delta,source
   INTO v_tax_id,v_tax_amount,v_source FROM treasury_ledger
   WHERE event_key=p_tax_event_key LIMIT 1 FOR UPDATE;
 IF v_tax_id IS NULL OR v_tax_amount<=0 OR BINARY v_source<>BINARY 'tax' THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='tax source not a verified receipt';
 END IF;
 IF BINARY p_event_key<>BINARY CONCAT('fund:tax:',v_tax_id) THEN
   SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='tax earmark event not canonical';
 END IF;
 SELECT tax_ledger_id,fund_event_key,contribution
   INTO v_existing,v_existing_event,v_amount FROM government_bond_tax_earmarks
   WHERE tax_ledger_id=v_tax_id LIMIT 1;
 IF v_existing IS NOT NULL THEN
   IF BINARY v_existing_event<>BINARY p_event_key THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='tax contribution event collision';
   END IF;
   SET v_replayed=1;
 ELSE
   IF v_enabled<>1 OR v_version>=2147483646 OR
      v_balance<v_min+v_fund+v_payout THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='tax reserve unavailable';
   END IF;
   SET v_amount=(CAST(v_tax_amount AS UNSIGNED)*v_rate) DIV 10000;
   IF v_amount>v_balance-v_min-v_fund-v_payout THEN
     SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='tax contribution exceeds free money';
   END IF;
   IF v_amount>0 THEN
     UPDATE treasury_accounts SET
       bond_fund_reserved=bond_fund_reserved+v_amount,
       row_version=row_version+1 WHERE code='state';
     SET v_fund=v_fund+v_amount;
     SET v_version=v_version+1;
     INSERT INTO bond_fund_ledger
       (event_key,operation,reference_code,reserve_delta,commitment_delta,
        reserved_after,committed_after,treasury_row_version_after)
       VALUES(p_event_key,'TAX_EARMARK',p_tax_event_key,
              v_amount,0,v_fund,v_committed,v_version);
   END IF;
   INSERT INTO government_bond_tax_earmarks
     (tax_ledger_id,fund_event_key,tax_event_key,receipt_amount,contribution,
      applied_rate_bps,treasury_row_version_after)
     VALUES(v_tax_id,p_event_key,p_tax_event_key,v_tax_amount,
            v_amount,v_rate,v_version);
 END IF;
 COMMIT;
 SELECT v_tax_id AS tax_ledger_id,v_amount AS contribution,
        v_fund AS fund_reserved,v_replayed AS replayed;
END$$
DELIMITER ;

INSERT INTO schema_migrations(version) VALUES (23);
