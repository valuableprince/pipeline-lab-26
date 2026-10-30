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
