-- GOV-07.2: correct coupon PayDay hour guard for non-UTC MySQL sessions.
-- Migration 027 is historical and must not be edited after deployment.
-- MySQL UNIX_TIMESTAMP(UTC_TIMESTAMP()) interprets UTC DATETIME in session TZ
-- and rejects a valid current UTC Unix hour when @@session.time_zone=+03:00.
-- UNIX_TIMESTAMP() without arguments returns a timezone-independent Unix epoch.
-- Apply after 027 on isolated staging while the game server is stopped.
-- No financial table data, historic coupon events or holdings are modified.

DELIMITER $$
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

DELIMITER ;
INSERT INTO schema_migrations(version) VALUES (28);
