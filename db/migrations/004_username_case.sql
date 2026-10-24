-- TARKENSO schema v4: preserve display-case username, prevent case-only duplicates.
-- Run once on an existing schema v3, after checking for duplicated LOWER(username).
-- Before migration: SELECT LOWER(username), COUNT(*) FROM accounts GROUP BY LOWER(username) HAVING COUNT(*)>1;
ALTER TABLE accounts
  ADD COLUMN username_key VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin
    GENERATED ALWAYS AS (LOWER(username)) STORED,
  ADD CONSTRAINT uq_accounts_username_key UNIQUE (username_key);

INSERT INTO schema_migrations (version) VALUES (4);
