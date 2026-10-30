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
