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
