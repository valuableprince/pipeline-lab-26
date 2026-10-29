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

INSERT INTO schema_migrations (version) VALUES (33);
