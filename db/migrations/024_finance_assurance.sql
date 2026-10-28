-- GOV-06: financial assurance, append-only evidence, restricted SQL privilege support.
-- Migrated ONLY after 023; do not run on production before final user acceptance.
-- IMPORTANT: execute migration with a permanent, dedicated financial DBA definer.
-- ALTER PROCEDURE does not change stored DEFINER ownership; verify using
-- information_schema.ROUTINES and require a non-superuser deployment definer.
-- Existing primary game DB user cannot be treated as an untrusted financial role.

ALTER PROCEDURE treasury_post SQL SECURITY DEFINER;
ALTER PROCEDURE gov_transfer_post SQL SECURITY DEFINER;
ALTER PROCEDURE bond_fund_capitalize SQL SECURITY DEFINER;
ALTER PROCEDURE bond_fund_commit SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_issue_create SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_issue_approve SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_issue_open SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_issue_cancel SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_buy SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_maturity_settle SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_insurance_assess SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_claim_collect SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_recovery_allocate SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_restructure_propose SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_restructure_approve SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_crisis_reconcile SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_issue_create_auto SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_official_approve SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_official_open SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_official_cancel SQL SECURITY DEFINER;
ALTER PROCEDURE gov_bond_tax_earmark SQL SECURITY DEFINER;

DELIMITER $$
CREATE TRIGGER g06_treasury_ledger_upd BEFORE UPDATE ON treasury_ledger
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_treasury_ledger_del BEFORE DELETE ON treasury_ledger
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_fund_ledger_upd BEFORE UPDATE ON bond_fund_ledger
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_fund_ledger_del BEFORE DELETE ON bond_fund_ledger
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_payout_ledger_upd BEFORE UPDATE ON bond_payout_ledger
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_payout_ledger_del BEFORE DELETE ON bond_payout_ledger
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_bond_issue_audit_upd BEFORE UPDATE ON government_bond_issue_audit
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_bond_issue_audit_del BEFORE DELETE ON government_bond_issue_audit
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_bond_orders_upd BEFORE UPDATE ON government_bond_orders
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_bond_orders_del BEFORE DELETE ON government_bond_orders
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_bond_claim_receipts_upd BEFORE UPDATE ON government_bond_claim_receipts
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_bond_claim_receipts_del BEFORE DELETE ON government_bond_claim_receipts
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_bond_recoveries_upd BEFORE UPDATE ON government_bond_recoveries
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_bond_recoveries_del BEFORE DELETE ON government_bond_recoveries
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_bond_restructure_audit_upd BEFORE UPDATE ON government_bond_restructure_audit
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_bond_restructure_audit_del BEFORE DELETE ON government_bond_restructure_audit
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$


CREATE TRIGGER g06_crisis_events_upd BEFORE UPDATE ON bond_crisis_events
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_crisis_events_del BEFORE DELETE ON bond_crisis_events
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_bond_tax_earmarks_upd BEFORE UPDATE ON government_bond_tax_earmarks
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$
CREATE TRIGGER g06_bond_tax_earmarks_del BEFORE DELETE ON government_bond_tax_earmarks
FOR EACH ROW BEGIN
  SIGNAL SQLSTATE '45000'
    SET MESSAGE_TEXT='GOV-06 immutable finance audit: corrections must use new events';
END$$

-- Integrity test is read-only and independent of the admin/issuance UI.
-- A nonzero mismatch FAILS by SIGNAL: no false "healthy" result.
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
       <> (SELECT COALESCE(SUM(o.quantity),0)
             FROM government_bond_orders o WHERE o.issue_id=i.issue_id);
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

INSERT INTO schema_migrations(version) VALUES (24);
