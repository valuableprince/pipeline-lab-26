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
