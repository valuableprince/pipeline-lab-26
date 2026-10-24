-- PAYDAY-01. Apply once after migration 006 on the hosting MariaDB.
-- Existing accounts start at level 1 with zero EXP; no historical XP is invented.
-- Preserve old NPC/route tables for possible future reuse (no destructive DDL).
ALTER TABLE accounts
  ADD COLUMN character_level INT UNSIGNED NOT NULL DEFAULT 1,
  ADD COLUMN character_exp INT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN last_payday_hour INT UNSIGNED NOT NULL DEFAULT 0;

INSERT INTO schema_migrations (version) VALUES (7);
