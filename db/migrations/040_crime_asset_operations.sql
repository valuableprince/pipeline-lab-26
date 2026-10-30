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
    (asset_code=1 AND influence_delta=2 AND standing_delta=0)
    OR (asset_code=2 AND influence_delta=0 AND standing_delta=2)
  )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO schema_migrations(version) VALUES (40);
