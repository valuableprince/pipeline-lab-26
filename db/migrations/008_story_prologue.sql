-- STORY-PROLOGUE-01. Apply after 007_payday.sql.
-- Forward-only character progress for "Signal 03:17 / Room 17".
-- No player progress, NPC tables, money or EXP is reset.
ALTER TABLE accounts
  ADD COLUMN story_stage TINYINT UNSIGNED NOT NULL DEFAULT 0;

INSERT INTO schema_migrations (version) VALUES (8);
