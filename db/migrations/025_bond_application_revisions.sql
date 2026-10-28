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

