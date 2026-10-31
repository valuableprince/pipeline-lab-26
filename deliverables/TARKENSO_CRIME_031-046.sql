-- TARKENSO CRIME migrations 031-046 (single-file phpMyAdmin import)
-- Source repository: valuableprince/pipeline-lab-26
-- Source revision: 994f5ea9e2394ed57b588516cc78ba7cc8d2b91a
-- IMPORTANT: Run ONLY once; requires base migrations 001-030, stopped server and a backup.
-- The DDL is NOT atomic: if any statement fails, restore backup before retrying.
-- Select the correct existing database BEFORE import; this bundle does not CREATE DATABASE.
SET NAMES utf8mb4;

-- ============================================================================
-- BEGIN 031_crime_syndicates.sql
-- ============================================================================
-- CRIME-01: three criminal syndicates without reassigning existing faction IDs.
-- Apply once after migration 030, with the game server stopped.
-- 1-3: legacy civil organizations; 4: reserved; 5: Ministry of Finance.
-- 6-8: criminal organizations. Rank model enforced by Leader_MaxRank in Pawn.
START TRANSACTION;
INSERT INTO factions (faction_id, is_enabled, term_start, term_end) VALUES
    (6, 1, 0, 0),
    (7, 1, 0, 0),
    (8, 1, 0, 0);
INSERT INTO schema_migrations (version) VALUES (31);
COMMIT;
-- END 031_crime_syndicates.sql

-- ============================================================================
-- BEGIN 032_crime_contract_core.sql
-- ============================================================================
-- CRIME-02.1 | solo contracts core. Apply once after 031, with server stopped.
-- MySQL 8.0.16+ / MariaDB 10.6+. All tables InnoDB/utf8mb4.
-- State: 1=ACCEPTED, 2=PICKED_UP, 3=COMPLETED, 4=EXPIRED, 5=ABORTED.
-- Database owns contract lifecycle, payout and unique active contract per account.

CREATE TABLE crime_contract_templates (
  template_id SMALLINT UNSIGNED NOT NULL PRIMARY KEY,
  code VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  title VARCHAR(64) NOT NULL,
  mode_id TINYINT UNSIGNED NOT NULL DEFAULT 1,
  reward_amount INT UNSIGNED NOT NULL,
  duration_seconds INT UNSIGNED NOT NULL,
  pickup_x DECIMAL(10,3) NOT NULL,
  pickup_y DECIMAL(10,3) NOT NULL,
  pickup_z DECIMAL(10,3) NOT NULL,
  delivery_x DECIMAL(10,3) NOT NULL,
  delivery_y DECIMAL(10,3) NOT NULL,
  delivery_z DECIMAL(10,3) NOT NULL,
  is_enabled TINYINT UNSIGNED NOT NULL DEFAULT 1,
  CONSTRAINT uq_crime_template_code UNIQUE (code),
  CONSTRAINT chk_crime_template_mode CHECK (mode_id IN (1,2,3)),
  CONSTRAINT chk_crime_template_reward CHECK (reward_amount BETWEEN 1 AND 50000),
  CONSTRAINT chk_crime_template_duration CHECK (duration_seconds BETWEEN 120 AND 7200),
  CONSTRAINT chk_crime_template_enabled CHECK (is_enabled IN (0,1))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE crime_contracts (
  contract_id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  template_id SMALLINT UNSIGNED NOT NULL,
  account_id BIGINT UNSIGNED NOT NULL,
  faction_id TINYINT UNSIGNED NOT NULL,
  mode_id TINYINT UNSIGNED NOT NULL,
  state TINYINT UNSIGNED NOT NULL DEFAULT 1,
  reward_amount INT UNSIGNED NOT NULL,
  started_at INT UNSIGNED NOT NULL,
  expires_at INT UNSIGNED NOT NULL,
  picked_up_at INT UNSIGNED NOT NULL DEFAULT 0,
  finished_at INT UNSIGNED NOT NULL DEFAULT 0,
  active_account_id BIGINT UNSIGNED GENERATED ALWAYS AS
    (CASE WHEN state IN (1,2) THEN account_id ELSE NULL END) STORED,
  UNIQUE KEY uq_crime_one_active_contract (active_account_id),
  KEY idx_crime_contract_account (account_id, contract_id),
  KEY idx_crime_contract_faction (faction_id, started_at),
  CONSTRAINT fk_crime_contract_template FOREIGN KEY (template_id)
    REFERENCES crime_contract_templates(template_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_contract_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_contract_faction FOREIGN KEY (faction_id)
    REFERENCES factions(faction_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_contract_state CHECK (state BETWEEN 1 AND 5),
  CONSTRAINT chk_crime_contract_mode CHECK (mode_id IN (1,2,3)),
  CONSTRAINT chk_crime_contract_reward CHECK (reward_amount BETWEEN 1 AND 50000),
  CONSTRAINT chk_crime_contract_clock CHECK (expires_at > started_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE crime_contract_events (
  event_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  contract_id INT UNSIGNED NOT NULL,
  actor_account_id BIGINT UNSIGNED NOT NULL,
  event_type VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE KEY uq_crime_contract_event (contract_id, event_type),
  KEY idx_crime_actor_events (actor_account_id, event_id),
  CONSTRAINT fk_crime_event_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_event_actor FOREIGN KEY (actor_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE crime_contract_rewards (
  contract_id INT UNSIGNED NOT NULL PRIMARY KEY,
  account_id BIGINT UNSIGNED NOT NULL,
  amount INT UNSIGNED NOT NULL,
  balance_after BIGINT UNSIGNED NOT NULL,
  row_version_after BIGINT UNSIGNED NOT NULL,
  txn_serial_after BIGINT UNSIGNED NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT fk_crime_reward_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_reward_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_reward_amount CHECK (amount BETWEEN 1 AND 50000),
  CONSTRAINT chk_crime_reward_balance CHECK (balance_after <= 1000000000)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Pilot noncompetitive solo operation: the route is a UAT location draft.
-- Its positions can be adjusted independently in the template table.
INSERT INTO crime_contract_templates
  (template_id,code,title,mode_id,reward_amount,duration_seconds,
   pickup_x,pickup_y,pickup_z,delivery_x,delivery_y,delivery_z)
VALUES
  (1,'SOLO_PORT_01','Ночной рейс',1,2000,1800,
   2765.000,-2410.000,13.500,2220.000,-2385.000,13.500);

INSERT INTO schema_migrations (version) VALUES (32);
-- END 032_crime_contract_core.sql

-- ============================================================================
-- BEGIN 033_crime_npc_contacts.sql
-- ============================================================================
-- CRIME-02.2 | NPC contacts, persistent trust and local solo offers.
-- Apply exactly once after 032, while the game server is stopped.
-- Coordinates/skins are UAT drafts and must be verified in GTA interiors.
CREATE TABLE crime_contacts (
  contact_id TINYINT UNSIGNED NOT NULL PRIMARY KEY,
  name VARCHAR(64) NOT NULL,
  city VARCHAR(32) NOT NULL,
  actor_model SMALLINT UNSIGNED NOT NULL,
  pos_x DECIMAL(10,3) NOT NULL,
  pos_y DECIMAL(10,3) NOT NULL,
  pos_z DECIMAL(10,3) NOT NULL,
  facing DECIMAL(6,2) NOT NULL DEFAULT 0,
  is_enabled TINYINT UNSIGNED NOT NULL DEFAULT 1,
  CONSTRAINT chk_crime_contacts_enabled CHECK (is_enabled IN (0,1))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE crime_contact_progress (
  account_id BIGINT UNSIGNED NOT NULL,
  contact_id TINYINT UNSIGNED NOT NULL,
  trust TINYINT UNSIGNED NOT NULL DEFAULT 0,
  completed_count INT UNSIGNED NOT NULL DEFAULT 0,
  first_met_at INT UNSIGNED NOT NULL,
  last_met_at INT UNSIGNED NOT NULL,
  PRIMARY KEY (account_id,contact_id),
  CONSTRAINT fk_crime_progress_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_progress_contact FOREIGN KEY (contact_id)
    REFERENCES crime_contacts(contact_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_progress_trust CHECK (trust BETWEEN 0 AND 100)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE crime_contact_trust_events (
  contract_id INT UNSIGNED NOT NULL PRIMARY KEY,
  account_id BIGINT UNSIGNED NOT NULL,
  contact_id TINYINT UNSIGNED NOT NULL,
  trust_delta TINYINT UNSIGNED NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  KEY idx_crime_trust_actor (account_id,contact_id),
  CONSTRAINT fk_crime_trust_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_trust_progress FOREIGN KEY (account_id,contact_id)
    REFERENCES crime_contact_progress(account_id,contact_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_trust_delta CHECK (trust_delta BETWEEN 1 AND 10)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO crime_contacts
  (contact_id,name,city,actor_model,pos_x,pos_y,pos_z,facing)
VALUES
  (1,'Виктор Седой','Лос-Сантос',50,2770.000,-2445.000,13.600,88.00),
  (2,'Эдвард Кейн','Сан-Фиерро',59,-1720.000,1345.000,7.200,195.00),
  (3,'Марта Рейнс','Лас-Вентурас',141,2145.000,1635.000,11.100,270.00);

-- Old accepted contracts and the pilot template are assigned to contact #1.
ALTER TABLE crime_contract_templates
  ADD COLUMN contact_id TINYINT UNSIGNED NOT NULL DEFAULT 1,
  ADD COLUMN required_trust TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD KEY idx_crime_template_contact (contact_id,is_enabled),
  ADD CONSTRAINT fk_crime_template_contact FOREIGN KEY (contact_id)
    REFERENCES crime_contacts(contact_id) ON DELETE RESTRICT,
  ADD CONSTRAINT chk_crime_template_trust CHECK (required_trust BETWEEN 0 AND 100);

ALTER TABLE crime_contracts
  ADD COLUMN contact_id TINYINT UNSIGNED NOT NULL DEFAULT 1,
  ADD KEY idx_crime_instance_contact (contact_id,account_id),
  ADD CONSTRAINT fk_crime_instance_contact FOREIGN KEY (contact_id)
    REFERENCES crime_contacts(contact_id) ON DELETE RESTRICT;

-- Different city routes; same delivery engine. Varied mission types are CRIME-02.3.
INSERT INTO crime_contract_templates
  (template_id,code,title,mode_id,reward_amount,duration_seconds,
   pickup_x,pickup_y,pickup_z,delivery_x,delivery_y,delivery_z,contact_id,required_trust)
VALUES
  (2,'SOLO_SF_01','Деловая посылка',1,2200,1800,
   -1690.000,1300.000,7.200,-1850.000,1150.000,45.000,2,0),
  (3,'SOLO_LV_01','Закрытый конверт',1,2100,1800,
   2120.000,1650.000,11.100,1980.000,1750.000,12.000,3,0);

-- Existing mission holders must be able to complete after the upgrade.
INSERT IGNORE INTO crime_contact_progress
  (account_id,contact_id,trust,completed_count,first_met_at,last_met_at)
SELECT DISTINCT account_id,1,0,0,UNIX_TIMESTAMP(),UNIX_TIMESTAMP()
FROM crime_contracts WHERE contact_id=1;

INSERT INTO schema_migrations (version) VALUES (33);
-- END 033_crime_npc_contacts.sql

-- ============================================================================
-- BEGIN 034_crime_solo_scenarios.sql
-- ============================================================================
-- CRIME-02.3: three distinct solo investigations / negotiations with durable choices.
-- Apply exactly once after 033 on stopped server after a verified backup.
ALTER TABLE crime_contract_templates
  ADD COLUMN scenario_kind TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD CONSTRAINT chk_crime_scenario_template CHECK (scenario_kind BETWEEN 0 AND 3);
ALTER TABLE crime_contracts
  ADD COLUMN scenario_kind TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN scenario_choice TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN scenario_resolution TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD CONSTRAINT chk_crime_scenario_kind CHECK (scenario_kind BETWEEN 0 AND 3),
  ADD CONSTRAINT chk_crime_scenario_choice CHECK (scenario_choice BETWEEN 0 AND 2),
  ADD CONSTRAINT chk_crime_scenario_resolution CHECK (scenario_resolution BETWEEN 0 AND 2);

-- 0=courier; 1=missing person, 2=warehouse investigation, 3=negotiation.
-- Existing template ids 1-3 and already accepted contracts retain kind=0.
-- Starting at 5 trust encourages a first ordinary job and NPC relationship.
INSERT INTO crime_contract_templates
 (template_id,code,title,mode_id,reward_amount,duration_seconds,
  pickup_x,pickup_y,pickup_z,delivery_x,delivery_y,delivery_z,
  contact_id,required_trust,scenario_kind)
VALUES
 (4,'SOLO_SF_MISSING','Пропавший курьер',1,2800,2400,
  -1725.000,1260.000,7.200,-1810.000,1205.000,25.100,2,5,1),
 (5,'SOLO_LV_WAREHOUSE','Подозрительный склад',1,2700,2400,
  2190.000,1580.000,11.100,2040.000,1700.000,12.000,3,5,2),
 (6,'SOLO_LS_DISPUTE','Сорванная сделка',1,2600,2400,
  2720.000,-2380.000,13.500,2280.000,-2350.000,13.500,1,5,3);

INSERT INTO schema_migrations (version) VALUES (34);
-- END 034_crime_solo_scenarios.sql

-- ============================================================================
-- BEGIN 035_crime_replay_variants.sql
-- ============================================================================
-- CRIME-02.4 | repeatable solo missions, seeded scenario circumstances,
-- cooldowns and future one-time story contracts. Apply once after 034.
-- Never deletes existing contract rows or rewards.
ALTER TABLE crime_contract_templates
  ADD COLUMN cooldown_seconds INT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN is_repeatable TINYINT UNSIGNED NOT NULL DEFAULT 1,
  ADD CONSTRAINT chk_crime_template_cooldown CHECK (cooldown_seconds BETWEEN 0 AND 86400),
  ADD CONSTRAINT chk_crime_template_repeatable CHECK (is_repeatable IN (0,1));

ALTER TABLE crime_contracts
  ADD COLUMN scenario_event TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD KEY idx_crime_repeat_history (account_id,template_id,state,finished_at);

-- Existing ongoing investigations need a stable circumstance on upgrade.
UPDATE crime_contracts
SET scenario_event=1+MOD(contract_id,3)
WHERE scenario_kind BETWEEN 1 AND 3 AND scenario_event=0;

ALTER TABLE crime_contracts
  ADD CONSTRAINT chk_crime_event_variant CHECK (
    (scenario_kind=0 AND scenario_event=0) OR
    (scenario_kind BETWEEN 1 AND 3 AND scenario_event BETWEEN 1 AND 3)
  );

-- Courier tasks can be repeated after five minutes;
-- investigations/negotiations after fifteen minutes.
UPDATE crime_contract_templates SET cooldown_seconds=300
WHERE template_id IN (1,2,3);
UPDATE crime_contract_templates SET cooldown_seconds=900
WHERE template_id IN (4,5,6);

-- is_repeatable=0 is reserved for future unique narrative contracts.
-- Eligibility is checked in the atomic acceptance INSERT, not only UI.
INSERT INTO schema_migrations (version) VALUES (35);
-- END 035_crime_replay_variants.sql

-- ============================================================================
-- BEGIN 036_crime_world_evidence.sql
-- ============================================================================
-- CRIME-02.5 | physical evidence interaction and witnesses.
-- Apply once after 035, with stopped server / verified DB backup.
-- One evidence discovery per accepted scenario, durable across relog.
ALTER TABLE crime_contracts
  ADD COLUMN evidence_found_at INT UNSIGNED NOT NULL DEFAULT 0;

-- Existing scenario missions on second stage must remain finishable.
-- Earlier stages already recorded CLUE_FOUND; backfill that historical proof.
UPDATE crime_contracts c
JOIN crime_contract_events e ON e.contract_id=c.contract_id
  AND e.event_type='CLUE_FOUND'
SET c.evidence_found_at=GREATEST(1,c.picked_up_at)
WHERE c.scenario_kind BETWEEN 1 AND 3
  AND c.state IN (2,3)
  AND c.evidence_found_at=0;

-- World interactions are indexed by unique (contract_id,event_type) from 032.
-- An evidence discovery is stored transactionally with its one-time event.
INSERT INTO schema_migrations (version) VALUES (36);
-- END 036_crime_world_evidence.sql

-- ============================================================================
-- BEGIN 037_crime_witness_interview.sql
-- ============================================================================
-- CRIME-02.6 | witness conversations and evidence-driven testimony.
-- Apply once after 036 with server stopped and database backed up.
ALTER TABLE crime_contracts
  ADD COLUMN interview_question TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN interviewed_at INT UNSIGNED NOT NULL DEFAULT 0,
  ADD CONSTRAINT chk_crime_interview_question CHECK (interview_question BETWEEN 0 AND 2);

-- Preserve all existing accepted missions. A player who already reached
-- the final stage before this migration can complete it without a forced
-- return to the scene. Question 0 means legacy/no interview-quality bonus.
UPDATE crime_contracts
SET interviewed_at=GREATEST(1,picked_up_at)
WHERE scenario_kind BETWEEN 1 AND 3
  AND state IN (2,3)
  AND interviewed_at=0;

INSERT INTO schema_migrations (version) VALUES (37);
-- END 037_crime_witness_interview.sql

-- ============================================================================
-- BEGIN 038_crime_npc_heat.sql
-- ============================================================================
-- CRIME-02.7 | local NPC heat, risk and trust-independent solo recovery.
-- Migration is incremental and must be applied only once after 037.
-- heat 0..100 is per account AND per contact; no other factions are affected.
ALTER TABLE crime_contact_progress
  ADD COLUMN heat TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN heat_event_count INT UNSIGNED NOT NULL DEFAULT 0,
  ADD CONSTRAINT chk_crime_contact_heat CHECK (heat BETWEEN 0 AND 100);

ALTER TABLE crime_contracts
  ADD COLUMN risk_delta SMALLINT NOT NULL DEFAULT 0,
  ADD CONSTRAINT chk_crime_contract_risk CHECK (risk_delta BETWEEN -12 AND 20);

CREATE TABLE crime_contact_heat_events (
  contract_id INT UNSIGNED NOT NULL PRIMARY KEY,
  account_id BIGINT UNSIGNED NOT NULL,
  contact_id TINYINT UNSIGNED NOT NULL,
  heat_delta SMALLINT NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  KEY idx_crime_heat_contact (account_id,contact_id),
  CONSTRAINT fk_crime_heat_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_heat_progress FOREIGN KEY (account_id,contact_id)
    REFERENCES crime_contact_progress(account_id,contact_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_heat_delta CHECK (heat_delta BETWEEN -12 AND 20)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- History before this migration has no heat events: all existing progress starts at zero.
-- Only future successful, settled contracts adjust heat, exactly once in a SQL transaction.
INSERT INTO schema_migrations (version) VALUES (38);
-- END 038_crime_npc_heat.sql

-- ============================================================================
-- BEGIN 039_crime_syndicate_influence.sql
-- ============================================================================
-- CRIME-02.8: persistent syndicate influence, contact relations, two assets.
-- Apply ONCE after 038 with server stopped and verified DB backup.
-- No money is minted here. Influence is an organization-specific, non-transferable resource.
CREATE TABLE crime_syndicate_progress (
  faction_id TINYINT UNSIGNED NOT NULL PRIMARY KEY,
  influence INT UNSIGNED NOT NULL DEFAULT 0,
  completed_count INT UNSIGNED NOT NULL DEFAULT 0,
  CONSTRAINT fk_crime_syndicate_progress FOREIGN KEY (faction_id)
    REFERENCES factions(faction_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_syndicate_influence CHECK (influence <= 1000000)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE crime_syndicate_contacts (
  faction_id TINYINT UNSIGNED NOT NULL,
  contact_id TINYINT UNSIGNED NOT NULL,
  standing TINYINT UNSIGNED NOT NULL DEFAULT 0,
  completed_count INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (faction_id,contact_id),
  CONSTRAINT fk_crime_syndicate_contact_faction FOREIGN KEY (faction_id)
    REFERENCES crime_syndicate_progress(faction_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_syndicate_contact_npc FOREIGN KEY (contact_id)
    REFERENCES crime_contacts(contact_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_contact_standing CHECK (standing BETWEEN 0 AND 100)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE crime_syndicate_assets (
  faction_id TINYINT UNSIGNED NOT NULL,
  contact_id TINYINT UNSIGNED NOT NULL,
  asset_code TINYINT UNSIGNED NOT NULL,
  cost_influence INT UNSIGNED NOT NULL,
  acquired_by BIGINT UNSIGNED NOT NULL,
  acquired_at INT UNSIGNED NOT NULL,
  PRIMARY KEY (faction_id,contact_id,asset_code),
  CONSTRAINT fk_crime_asset_contact FOREIGN KEY (faction_id,contact_id)
    REFERENCES crime_syndicate_contacts(faction_id,contact_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_asset_actor FOREIGN KEY (acquired_by)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_asset_code CHECK (asset_code IN(1,2)),
  CONSTRAINT chk_crime_asset_cost CHECK (cost_influence IN(15,30))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE crime_syndicate_influence_events (
  contract_id INT UNSIGNED NOT NULL PRIMARY KEY,
  faction_id TINYINT UNSIGNED NOT NULL,
  contact_id TINYINT UNSIGNED NOT NULL,
  influence_delta TINYINT UNSIGNED NOT NULL,
  standing_delta TINYINT UNSIGNED NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT fk_crime_influence_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_influence_contact FOREIGN KEY (faction_id,contact_id)
    REFERENCES crime_syndicate_contacts(faction_id,contact_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_influence_delta CHECK (influence_delta BETWEEN 1 AND 10),
  CONSTRAINT chk_crime_standing_delta CHECK (standing_delta BETWEEN 1 AND 10)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO crime_syndicate_progress (faction_id)
  SELECT faction_id FROM factions WHERE faction_id IN (6,7,8);
INSERT INTO crime_syndicate_contacts (faction_id,contact_id)
  SELECT p.faction_id,c.contact_id FROM crime_syndicate_progress p
  CROSS JOIN crime_contacts c WHERE c.is_enabled=1;
-- Prior contracts are historical and do not retroactively earn influence.
INSERT INTO schema_migrations (version) VALUES (39);
-- END 039_crime_syndicate_influence.sql

-- ============================================================================
-- BEGIN 040_crime_asset_operations.sql
-- ============================================================================
-- CRIME-02.9 | world-based syndicate asset assignments.
-- Apply once after 039 with the server stopped and a verified DB backup.
-- Each purchased asset can be serviced by any active member of its faction.
-- The first successful interaction activates its contract bonus; subsequent
-- operations are available after 30 minutes per asset (not per player).
ALTER TABLE crime_syndicate_assets
  ADD COLUMN activated_at INT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN last_serviced_at INT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN service_count INT UNSIGNED NOT NULL DEFAULT 0;

-- Keep historical asset bonuses working after upgrade.
UPDATE crime_syndicate_assets
SET activated_at=acquired_at WHERE activated_at=0;

CREATE TABLE crime_syndicate_asset_services (
  faction_id TINYINT UNSIGNED NOT NULL,
  contact_id TINYINT UNSIGNED NOT NULL,
  asset_code TINYINT UNSIGNED NOT NULL,
  service_count INT UNSIGNED NOT NULL,
  actor_account_id BIGINT UNSIGNED NOT NULL,
  influence_delta TINYINT UNSIGNED NOT NULL DEFAULT 0,
  standing_delta TINYINT UNSIGNED NOT NULL DEFAULT 0,
  serviced_at INT UNSIGNED NOT NULL,
  PRIMARY KEY (faction_id,contact_id,asset_code,service_count),
  KEY idx_crime_asset_services_actor (actor_account_id,serviced_at),
  CONSTRAINT fk_crime_service_asset FOREIGN KEY (faction_id,contact_id,asset_code)
    REFERENCES crime_syndicate_assets(faction_id,contact_id,asset_code)
    ON DELETE RESTRICT,
  CONSTRAINT fk_crime_service_actor FOREIGN KEY (actor_account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_service_delta CHECK (
    (asset_code=1 AND influence_delta BETWEEN 0 AND 2 AND standing_delta=0)
    OR (asset_code=2 AND influence_delta=0 AND standing_delta BETWEEN 0 AND 2)
  )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO schema_migrations(version) VALUES (40);
-- END 040_crime_asset_operations.sql

-- ============================================================================
-- BEGIN 041_crime_competitive_contracts.sql
-- ============================================================================
-- CRIME-03.1 | public timed contracts, first completed delivery wins.
-- One hourly round per NPC, competing faction entries share a global winner.
-- All money remains inside the existing crime_contracts transaction engine.
CREATE TABLE crime_competition_rounds (
  round_id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  contact_id TINYINT UNSIGNED NOT NULL,
  template_id SMALLINT UNSIGNED NOT NULL,
  hour_bucket INT UNSIGNED NOT NULL,
  opens_at INT UNSIGNED NOT NULL,
  expires_at INT UNSIGNED NOT NULL,
  state TINYINT UNSIGNED NOT NULL DEFAULT 1,
  winner_contract_id INT UNSIGNED DEFAULT NULL,
  winner_faction_id TINYINT UNSIGNED DEFAULT NULL,
  winner_account_id BIGINT UNSIGNED DEFAULT NULL,
  closed_at INT UNSIGNED NOT NULL DEFAULT 0,
  UNIQUE KEY uq_crime_comp_round (contact_id,hour_bucket),
  KEY idx_crime_comp_open (state,expires_at),
  CONSTRAINT fk_comp_contact FOREIGN KEY (contact_id)
    REFERENCES crime_contacts(contact_id) ON DELETE RESTRICT,
  CONSTRAINT fk_comp_template FOREIGN KEY (template_id)
    REFERENCES crime_contract_templates(template_id) ON DELETE RESTRICT,
  CONSTRAINT chk_comp_state CHECK (state IN(1,2,3)),
  CONSTRAINT chk_comp_time CHECK (expires_at>opens_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

ALTER TABLE crime_contracts
  ADD COLUMN competition_id INT UNSIGNED DEFAULT NULL,
  ADD UNIQUE KEY uq_crime_comp_faction (competition_id,faction_id),
  ADD KEY idx_crime_comp_entries (competition_id,state),
  ADD CONSTRAINT fk_crime_competition FOREIGN KEY (competition_id)
    REFERENCES crime_competition_rounds(round_id) ON DELETE RESTRICT;

-- Cross-faction competition, but still a distinct delivery route for each
-- city; route coordinates are UAT drafts and need verification in GTA.
INSERT INTO crime_contract_templates
  (template_id,code,title,mode_id,reward_amount,duration_seconds,
   pickup_x,pickup_y,pickup_z,delivery_x,delivery_y,delivery_z,
   contact_id,required_trust,scenario_kind,cooldown_seconds,is_repeatable)
VALUES
 (7,'RACE_LS_01','Портовый приоритет',2,3200,2700,
  2755.000,-2460.000,13.500,2390.000,-2290.000,13.500,1,5,0,0,1),
 (8,'RACE_SF_01','Перехват манифеста',2,3400,2700,
  -1740.000,1280.000,7.200,-1950.000,1175.000,45.000,2,5,0,0,1),
 (9,'RACE_LV_01','Дефицитный груз',2,3300,2700,
  2180.000,1610.000,11.100,1980.000,1820.000,12.000,3,5,0,0,1);

INSERT INTO schema_migrations(version) VALUES (41);
-- END 041_crime_competitive_contracts.sql

-- ============================================================================
-- BEGIN 042_crime_tender_offers.sql
-- ============================================================================
-- CRIME-03.2: NPC tender offers. Apply once after migration 041 with server stopped.
-- Bids cost no money. Only the awarded courier contract can mint a reward.
CREATE TABLE crime_tender_rounds (
  tender_id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  contact_id TINYINT UNSIGNED NOT NULL,
  template_id SMALLINT UNSIGNED NOT NULL,
  bucket INT UNSIGNED NOT NULL,
  opens_at INT UNSIGNED NOT NULL,
  closes_at INT UNSIGNED NOT NULL,
  state TINYINT UNSIGNED NOT NULL DEFAULT 1,
  winner_bid_id BIGINT UNSIGNED DEFAULT NULL,
  winner_contract_id INT UNSIGNED DEFAULT NULL,
  decided_at INT UNSIGNED NOT NULL DEFAULT 0,
  UNIQUE KEY uq_crime_tender_window (contact_id,bucket),
  KEY idx_crime_tender_pending (state,closes_at),
  CONSTRAINT fk_tender_npc FOREIGN KEY (contact_id)
    REFERENCES crime_contacts(contact_id) ON DELETE RESTRICT,
  CONSTRAINT fk_tender_template FOREIGN KEY (template_id)
    REFERENCES crime_contract_templates(template_id) ON DELETE RESTRICT,
  CONSTRAINT chk_tender_state CHECK (state IN(1,2,3)),
  CONSTRAINT chk_tender_clock CHECK (closes_at>opens_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE crime_tender_bids (
  bid_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  tender_id INT UNSIGNED NOT NULL,
  account_id BIGINT UNSIGNED NOT NULL,
  faction_id TINYINT UNSIGNED NOT NULL,
  offer_code TINYINT UNSIGNED NOT NULL,
  state TINYINT UNSIGNED NOT NULL DEFAULT 1,
  submitted_at INT UNSIGNED NOT NULL,
  pending_account_id BIGINT UNSIGNED GENERATED ALWAYS AS
    (CASE WHEN state=1 THEN account_id ELSE NULL END) STORED,
  UNIQUE KEY uq_tender_bid_faction (tender_id,faction_id),
  UNIQUE KEY uq_tender_bid_account (tender_id,account_id),
  UNIQUE KEY uq_tender_one_pending (pending_account_id),
  KEY idx_tender_priority (tender_id,state,offer_code,bid_id),
  CONSTRAINT fk_tender_bid_round FOREIGN KEY (tender_id)
    REFERENCES crime_tender_rounds(tender_id) ON DELETE RESTRICT,
  CONSTRAINT fk_tender_bid_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT fk_tender_bid_faction FOREIGN KEY (faction_id)
    REFERENCES factions(faction_id) ON DELETE RESTRICT,
  CONSTRAINT chk_tender_offer CHECK (offer_code IN(1,2,3)),
  CONSTRAINT chk_tender_bid_state CHECK (state IN(1,2,3))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

ALTER TABLE crime_contracts
  ADD COLUMN tender_id INT UNSIGNED DEFAULT NULL,
  ADD UNIQUE KEY uq_crime_tender_contract (tender_id),
  ADD CONSTRAINT fk_crime_tender_contract FOREIGN KEY (tender_id)
    REFERENCES crime_tender_rounds(tender_id) ON DELETE RESTRICT;

CREATE TABLE crime_tender_penalties (
  contract_id INT UNSIGNED NOT NULL PRIMARY KEY,
  account_id BIGINT UNSIGNED NOT NULL,
  faction_id TINYINT UNSIGNED NOT NULL,
  contact_id TINYINT UNSIGNED NOT NULL,
  reason TINYINT UNSIGNED NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT fk_tender_penalty_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT chk_tender_penalty_reason CHECK (reason IN(4,5))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Three distinct tenders, with actual pickup and delivery objectives.
-- Coordinates are drafts until final in-game UAT.
INSERT INTO crime_contract_templates
  (template_id,code,title,mode_id,reward_amount,duration_seconds,
   pickup_x,pickup_y,pickup_z,delivery_x,delivery_y,delivery_z,
   contact_id,required_trust,scenario_kind,cooldown_seconds,is_repeatable)
VALUES
 (10,'TENDER_LS_01','Тихая разгрузка',3,3600,2400,
  2760.000,-2415.000,13.500,2240.000,-2310.000,13.500,1,5,0,0,1),
 (11,'TENDER_SF_01','Закрытый контейнер',3,3800,2400,
  -1735.000,1265.000,7.200,-1990.000,1120.000,45.000,2,5,0,0,1),
 (12,'TENDER_LV_01','Секретная поставка',3,3700,2400,
  2195.000,1590.000,11.100,2010.000,1780.000,12.000,3,5,0,0,1);

INSERT INTO schema_migrations(version) VALUES (42);
-- END 042_crime_tender_offers.sql

-- ============================================================================
-- BEGIN 043_crime_tender_incidents.sql
-- ============================================================================
-- CRIME-03.3: durable NPC road encounters for tender deliveries.
-- Must be applied after 042, with the server stopped and a verified backup.
CREATE TABLE crime_tender_incidents (
  contract_id INT UNSIGNED NOT NULL PRIMARY KEY,
  incident_code TINYINT UNSIGNED NOT NULL,
  choice TINYINT UNSIGNED NOT NULL DEFAULT 0,
  resolved_at INT UNSIGNED NOT NULL DEFAULT 0,
  CONSTRAINT fk_tender_incident_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT chk_tender_incident_code CHECK (incident_code BETWEEN 1 AND 3),
  CONSTRAINT chk_tender_incident_choice CHECK (choice BETWEEN 0 AND 2),
  CONSTRAINT chk_tender_incident_resolution CHECK
    ((choice=0 AND resolved_at=0) OR (choice IN(1,2) AND resolved_at>0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Protect in-progress mode=3 contracts awarded before the 043 deployment.
-- Historical completed contracts need no retroactive incident or payout.
INSERT INTO crime_tender_incidents(contract_id,incident_code)
  SELECT contract_id,1+MOD(contract_id,3)
  FROM crime_contracts
  WHERE mode_id=3 AND state IN(1,2);

INSERT INTO schema_migrations(version) VALUES (43);
-- END 043_crime_tender_incidents.sql

-- ============================================================================
-- BEGIN 044_crime_tender_aftermath.sql
-- ============================================================================
-- CRIME-03.4: durable branch-dependent follow-up after the incident.
-- Apply only after 043 with the server stopped and a verified backup.
CREATE TABLE crime_tender_aftermath (
  contract_id INT UNSIGNED NOT NULL PRIMARY KEY,
  choice TINYINT UNSIGNED NOT NULL,
  state TINYINT UNSIGNED NOT NULL DEFAULT 1,
  finished_at INT UNSIGNED NOT NULL DEFAULT 0,
  CONSTRAINT fk_tender_aftermath_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT chk_tender_aftermath_choice CHECK (choice IN (1,2)),
  CONSTRAINT chk_tender_aftermath_state CHECK
    ((state=1 AND finished_at=0) OR (state=2 AND finished_at>0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Players whose incidents were completed before this deployment should
-- finish the new stage, rather than bypassing the new payout gate.
INSERT INTO crime_tender_aftermath(contract_id,choice)
  SELECT c.contract_id,i.choice FROM crime_contracts c
  JOIN crime_tender_incidents i ON i.contract_id=c.contract_id
  WHERE c.mode_id=3 AND c.state IN (1,2)
    AND i.choice IN (1,2) AND i.resolved_at>0;

INSERT INTO schema_migrations(version) VALUES (44);
-- END 044_crime_tender_aftermath.sql

-- ============================================================================
-- BEGIN 045_crime_intelligence_cases.sql
-- ============================================================================
-- CRIME-03.5: optional 30-minute broker intelligence from completed tenders.
-- Deploy after 044 with a stopped server and verified database backup.
CREATE TABLE crime_intelligence_cases (
  contract_id INT UNSIGNED NOT NULL PRIMARY KEY,
  puzzle_code TINYINT UNSIGNED NOT NULL,
  attempts SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  next_attempt_at INT UNSIGNED NOT NULL DEFAULT 0,
  solved_at INT UNSIGNED NOT NULL DEFAULT 0,
  decision TINYINT UNSIGNED NOT NULL DEFAULT 0,
  closed_at INT UNSIGNED NOT NULL DEFAULT 0,
  expires_at INT UNSIGNED NOT NULL,
  KEY idx_crime_intel_expires (expires_at),
  CONSTRAINT fk_crime_intel_contract FOREIGN KEY (contract_id)
    REFERENCES crime_contracts(contract_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_intel_puzzle CHECK (puzzle_code BETWEEN 1 AND 3),
  CONSTRAINT chk_crime_intel_decision CHECK (decision IN(0,1,2)),
  CONSTRAINT chk_crime_intel_closed CHECK
    ((decision=0 AND closed_at=0) OR
     (decision IN(1,2) AND closed_at>0 AND solved_at>0)),
  CONSTRAINT chk_crime_intel_deadline CHECK (expires_at>0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Restore opportunities for recent tenders completed before deployment.
-- Old completed tenders do not gain an indefinite or renewed reward window.
INSERT INTO crime_intelligence_cases(contract_id,puzzle_code,expires_at)
  SELECT c.contract_id,1+MOD(c.contract_id,3),c.finished_at+1800
  FROM crime_contracts c
  JOIN crime_contract_rewards r ON r.contract_id=c.contract_id
  WHERE c.mode_id=3 AND c.state=3
    AND c.finished_at>UNIX_TIMESTAMP()-1800;

INSERT INTO schema_migrations(version) VALUES (45);
-- END 045_crime_intelligence_cases.sql

-- ============================================================================
-- BEGIN 046_crime_contact_recovery.sql
-- ============================================================================
-- CRIME-03.6: solo NPC recovery missions when attention threatens the tender channel.
-- Apply once after 045 with the server stopped and a verified backup.
-- One active recovery across all contacts per account; one start per 60 min per contact.
CREATE TABLE crime_contact_recoveries (
  recovery_id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  account_id BIGINT UNSIGNED NOT NULL,
  faction_id TINYINT UNSIGNED NOT NULL,
  contact_id TINYINT UNSIGNED NOT NULL,
  method TINYINT UNSIGNED NOT NULL,
  state TINYINT UNSIGNED NOT NULL DEFAULT 1,
  started_at INT UNSIGNED NOT NULL,
  expires_at INT UNSIGNED NOT NULL,
  field_at INT UNSIGNED NOT NULL DEFAULT 0,
  finished_at INT UNSIGNED NOT NULL DEFAULT 0,
  heat_before TINYINT UNSIGNED NOT NULL DEFAULT 0,
  heat_after TINYINT UNSIGNED NOT NULL DEFAULT 0,
  standing_before TINYINT UNSIGNED NOT NULL DEFAULT 0,
  standing_after TINYINT UNSIGNED NOT NULL DEFAULT 0,
  active_account_id BIGINT UNSIGNED GENERATED ALWAYS AS
    (CASE WHEN state IN(1,2) THEN account_id ELSE NULL END) STORED,
  UNIQUE KEY uq_crime_one_active_recovery (active_account_id),
  KEY idx_crime_recovery_cooldown (account_id,contact_id,started_at),
  CONSTRAINT fk_crime_recovery_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_recovery_faction FOREIGN KEY (faction_id)
    REFERENCES factions(faction_id) ON DELETE RESTRICT,
  CONSTRAINT fk_crime_recovery_contact FOREIGN KEY (contact_id)
    REFERENCES crime_contacts(contact_id) ON DELETE RESTRICT,
  CONSTRAINT chk_crime_recovery_method CHECK (method IN(1,2)),
  CONSTRAINT chk_crime_recovery_state CHECK (state IN(1,2,3,4)),
  CONSTRAINT chk_crime_recovery_clock CHECK (expires_at>started_at),
  CONSTRAINT chk_crime_recovery_stages CHECK
    ((state=1 AND field_at=0 AND finished_at=0) OR
     (state=2 AND field_at>0 AND finished_at=0) OR
     (state=3 AND field_at>0 AND finished_at>0) OR
     (state=4 AND finished_at=0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO schema_migrations(version) VALUES (46);
-- END 046_crime_contact_recovery.sql

