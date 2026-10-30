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
