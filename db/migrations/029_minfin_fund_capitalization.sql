-- GOV-08: Ministry-managed capitalization of the protected insurance reserve.
-- Apply once AFTER 028 with game server stopped and a database backup.
-- This transfers NO cash: it earmarks existing unreserved state-treasury funds.
-- SQL authorization is checked INSIDE the same transaction as the fund entry.
-- Keep historical 019/024 procedures immutable; retain append-only ledger.
-- Use a dedicated permanent DBA definer in production; review SQL EXECUTE grants.

DELIMITER $$
DROP PROCEDURE IF EXISTS gov_bond_official_fund_capitalize$$
CREATE PROCEDURE gov_bond_official_fund_capitalize(
  IN p_event_key VARCHAR(64),
  IN p_actor BIGINT UNSIGNED,
  IN p_amount INT
)
SQL SECURITY DEFINER
BEGIN
  DECLARE v_balance BIGINT UNSIGNED DEFAULT NULL;
  DECLARE v_reserved BIGINT UNSIGNED;
  DECLARE v_committed BIGINT UNSIGNED;
  DECLARE v_minimum BIGINT UNSIGNED;
  DECLARE v_max_transaction BIGINT UNSIGNED;
  DECLARE v_version INT UNSIGNED;
  DECLARE v_rank TINYINT UNSIGNED DEFAULT NULL;
  DECLARE v_entry BIGINT UNSIGNED DEFAULT NULL;
  DECLARE v_operation VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin;
  DECLARE v_delta BIGINT;
  DECLARE v_actor BIGINT UNSIGNED;
  DECLARE v_replayed TINYINT UNSIGNED DEFAULT 0;

  DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
  IF p_event_key IS NULL OR CHAR_LENGTH(p_event_key) NOT BETWEEN 10 AND 64
     OR LEFT(BINARY p_event_key,10) <> BINARY 'fund:gov5:'
     OR p_event_key NOT REGEXP '^fund:gov5:[A-Za-z0-9_:/.-]+$'
     OR p_actor IS NULL OR p_actor=0
     OR p_amount IS NULL OR p_amount<1 OR p_amount>1000000000 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='invalid ministry fund request';
  END IF;

  START TRANSACTION;
  -- Preserve GOV-01 lock order: treasury first, then fund ledger.
  SELECT balance,bond_fund_reserved,bond_fund_committed,
         minimum_reserve,max_transaction,row_version
    INTO v_balance,v_reserved,v_committed,v_minimum,v_max_transaction,v_version
    FROM treasury_accounts WHERE code='state' FOR UPDATE;
  IF v_balance IS NULL THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='state treasury missing';
  END IF;

  SELECT m.rank_id INTO v_rank
    FROM faction_members m JOIN factions f ON f.faction_id=m.faction_id
    WHERE m.account_id=p_actor AND m.faction_id=5 AND m.rank_id=4
      AND f.is_enabled=1 AND f.term_end>UNIX_TIMESTAMP()
    LIMIT 1;
  IF v_rank IS NULL OR v_rank<>4 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='finance minister required';
  END IF;

  SELECT entry_id,operation,reserve_delta,actor_account_id
    INTO v_entry,v_operation,v_delta,v_actor
    FROM bond_fund_ledger WHERE event_key=p_event_key LIMIT 1;
  IF v_entry IS NOT NULL THEN
    IF v_operation <> 'CAPITALIZE' OR v_delta<>p_amount
       OR NOT (v_actor <=> p_actor) THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='ministry fund event collision';
    END IF;
    SET v_replayed=1;
  ELSE
    IF p_amount>v_max_transaction OR v_version>=2147483646
       OR v_reserved>v_balance OR v_minimum+v_reserved>v_balance
       OR v_balance-v_minimum-v_reserved<p_amount THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='not enough free treasury money';
    END IF;
    UPDATE treasury_accounts SET
      bond_fund_reserved=bond_fund_reserved+p_amount,
      row_version=row_version+1 WHERE code='state';
    SET v_reserved=v_reserved+p_amount;
    SET v_version=v_version+1;
    INSERT INTO bond_fund_ledger
      (event_key,operation,reference_code,reserve_delta,commitment_delta,
       reserved_after,committed_after,treasury_row_version_after,actor_account_id)
      VALUES(p_event_key,'CAPITALIZE',NULL,p_amount,0,
             v_reserved,v_committed,v_version,p_actor);
  END IF;
  COMMIT;
  SELECT v_balance AS treasury_balance,
         v_reserved AS fund_reserved,
         v_committed AS fund_committed,
         v_version AS row_version,
         p_amount AS applied_amount,
         p_actor AS actor_account_id,
         v_replayed AS replayed;
END$$
DELIMITER ;

INSERT INTO schema_migrations(version) VALUES (29);
