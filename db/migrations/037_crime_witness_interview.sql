-- CRIME-02.6 | witness conversations and evidence-driven testimony.
-- Apply once after 036 with server stopped and database backed up.
ALTER TABLE crime_contracts
  ADD COLUMN interview_question TINYINT UNSIGNED NOT NULL DEFAULT 0,
  ADD COLUMN interviewed_at INT UNSIGNED NOT NULL DEFAULT 0,
  ADD CONSTRAINT chk_crime_interview_question CHECK (interview_question BETWEEN 0 AND 2);

-- Preserve all existing accepted missions. A player who already reached
-- the final stage before this migration can complete it without a forced
-- return to the scene. Question 0 means legacy/no interview-quality bonus.
UPDATE crime_contracts
SET interviewed_at=GREATEST(1,picked_up_at)
WHERE scenario_kind BETWEEN 1 AND 3
  AND state IN (2,3)
  AND interviewed_at=0;

INSERT INTO schema_migrations (version) VALUES (37);
