-- LOADTEST-04: isolated synthetic registration/login/save benchmark records.
-- This table is deliberately NOT linked to real accounts, money, roles or PayDay.
-- Apply ONCE after migration 012.
CREATE TABLE loadtest_virtual_accounts (
    run_key VARCHAR(96) NOT NULL,
    virtual_id SMALLINT UNSIGNED NOT NULL,
    nickname VARCHAR(24) NOT NULL,
    password_hash CHAR(60) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    balance INT UNSIGNED NOT NULL DEFAULT 0,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (run_key, virtual_id),
    KEY ix_loadtest_virtual_created (created_at),
    CONSTRAINT chk_loadtest_virtual_id CHECK (virtual_id BETWEEN 0 AND 99)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO schema_migrations (version) VALUES (13);
