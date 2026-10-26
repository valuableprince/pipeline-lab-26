-- LOADNET-01. Apply once after migration 011 (TARKENSO MySQL).
-- Only one queued/running/stopping load test at a time; finished jobs remain as history.
CREATE TABLE loadtest_jobs (
  job_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  account_id BIGINT UNSIGNED NOT NULL,
  target_clients SMALLINT UNSIGNED NOT NULL,
  duration_seconds SMALLINT UNSIGNED NOT NULL,
  auth_mode ENUM('register','login') NOT NULL DEFAULT 'register',
  scenario ENUM('idle','commands') NOT NULL DEFAULT 'idle',
  state ENUM('PENDING','RUNNING','STOP_REQUESTED','COMPLETE','STOPPED','FAILED') NOT NULL DEFAULT 'PENDING',
  -- MySQL UNIQUE allows multiple NULL values, but only one active slot = 1.
  active_slot TINYINT UNSIGNED GENERATED ALWAYS AS
    (CASE WHEN state IN ('PENDING','RUNNING','STOP_REQUESTED') THEN 1 ELSE NULL END) STORED,
  requested_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  started_at TIMESTAMP NULL DEFAULT NULL,
  finished_at TIMESTAMP NULL DEFAULT NULL,
  heartbeat_at TIMESTAMP NULL DEFAULT NULL,
  launched_clients SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  joined_clients SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  authenticated_clients SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  spawned_clients SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  status_message VARCHAR(120) NOT NULL DEFAULT '',
  UNIQUE KEY uq_loadtest_one_active (active_slot),
  KEY ix_loadtest_state_id (state, job_id),
  CONSTRAINT fk_loadtest_requester FOREIGN KEY (account_id) REFERENCES accounts(account_id),
  CONSTRAINT chk_loadtest_clients CHECK (target_clients BETWEEN 1 AND 50),
  CONSTRAINT chk_loadtest_seconds CHECK (duration_seconds BETWEEN 10 AND 600)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO schema_migrations (version) VALUES (12);
