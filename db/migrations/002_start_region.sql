-- TARKENSO / START-REGION-01.
-- Run once AFTER 001_initial.sql; backup before updating live database.
-- Existing accounts receive Fort Carson (1) without changing other fields.
ALTER TABLE accounts
  ADD COLUMN start_region TINYINT UNSIGNED NOT NULL DEFAULT 1,
  ADD CONSTRAINT chk_accounts_start_region CHECK (start_region BETWEEN 1 AND 3);
INSERT INTO schema_migrations (version) VALUES (2);
