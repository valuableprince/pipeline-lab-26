-- ADMIN-PUNISH-03. Apply to existing schema v8 with the server STOPPED.
-- Tables are intentionally separate from accounts; existing character data is preserved.
CREATE TABLE admin_jail_sentences (
  sentence_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  account_id BIGINT UNSIGNED NOT NULL,
  issued_by VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  reason VARCHAR(96) NOT NULL,
  jailed_until INT UNSIGNED NOT NULL,
  issued_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  released_at TIMESTAMP NULL DEFAULT NULL,
  released_by VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT NULL,
  KEY idx_jail_account (account_id, released_at, sentence_id),
  CONSTRAINT fk_jail_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE admin_warnings (
  warning_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  account_id BIGINT UNSIGNED NOT NULL,
  issued_by VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  reason VARCHAR(96) NOT NULL,
  issued_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  revoked_at TIMESTAMP NULL DEFAULT NULL,
  revoked_by VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT NULL,
  revoked_reason VARCHAR(96) NULL DEFAULT NULL,
  KEY idx_warnings_account (account_id, revoked_at),
  CONSTRAINT fk_warning_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE admin_player_notes (
  note_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  account_id BIGINT UNSIGNED NOT NULL,
  author VARCHAR(20) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  note VARCHAR(160) NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  KEY idx_notes_account (account_id, note_id),
  CONSTRAINT fk_notes_account FOREIGN KEY (account_id)
    REFERENCES accounts(account_id) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO schema_migrations (version) VALUES (9);
