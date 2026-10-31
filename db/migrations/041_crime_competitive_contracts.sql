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
