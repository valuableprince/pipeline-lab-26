-- JOB-TRUCK-03 hotfix: bin and overflow are independent storage areas.
-- Version 10's chk_cc_full blocks emptying a full bin while bags remain
-- beside it. Keep bin/overflow individual range checks and editor validation.
-- Apply once via phpMyAdmin before deploying the new AMX.
-- Back up the cleaner_containers table first.
ALTER TABLE cleaner_containers DROP CONSTRAINT chk_cc_full;

INSERT INTO schema_migrations (version) VALUES (11);
