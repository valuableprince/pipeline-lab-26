# TARKENSO migration conflict: v12 TextDraw Studio vs. LoadNet

Canonical sequence from the verified TextDraw Studio SQL supplied by the owner:

- 011 - municipal truck delivery
- **012 - TextDraw Studio**: `012_tdstudio_projects.sql` (projects, elements, four `tds_*` procedures)
- **013 - in-game synthetic NPC account benchmarks**: `013_loadtest_virtual_accounts.sql`
- **014 - optional external PC Worker /loadnet**: `014_loadnet_jobs.sql`

The old `012_loadnet_jobs.sql` was removed from the repository.
No game hosting database has been modified by this repository change.

## Read-only inspection in phpMyAdmin

Back up the database first, then run:

```sql
SELECT version, applied_at FROM schema_migrations WHERE version IN (11,12,13,14) ORDER BY version;
SHOW TABLES LIKE 'tdstudio_projects';
SHOW TABLES LIKE 'tdstudio_elements';
SHOW TABLES LIKE 'loadtest_jobs';
SHOW TABLES LIKE 'loadtest_virtual_accounts';
SELECT ROUTINE_NAME FROM information_schema.ROUTINES
WHERE ROUTINE_SCHEMA = DATABASE() AND ROUTINE_TYPE='PROCEDURE'
  AND ROUTINE_NAME IN ('tds_save','tds_delete','tds_list','tds_load')
ORDER BY ROUTINE_NAME;
```

**A version row is not proof of the correct DDL being applied.**
If version 12 exists but `tdstudio_projects` / `tdstudio_elements`
and the expected procedures are absent, the old LoadNet migration might
have registered the wrong version. Do not overwrite, DROP or DELETE
objects on the basis of a version number.

- **TextDraw schema present and correct; version 12 present:** treat 12 as
  TextDraw; apply missing 013 for full NPC simulation. 014 is optional.
- **Both TextDraw and LoadNet objects exist, but only a 12 marker:** after
  independently verifying both schemas and backups, restore the missing
  canonical 14 marker with `INSERT INTO schema_migrations(version) VALUES(14);`
  (only if it does not already exist). Do not rerun either DDL.
- **Only LoadNet objects exist and version 12 is present:** database is
  inconsistent. Preserve all tables and data; apply the TextDraw SQL
  cautiously without its final duplicate version INSERT, only if its
  objects do not yet exist and prerequisites are met; once verified,
  record 14 for LoadNet. This requires a schema-specific recovery,
  not a generic destructive rollback.
- **Neither set of objects exists:** review applied versions and follow
  the canonical migration order, one file at a time.

Migration 012 contains MySQL stored procedures and `DELIMITER`
statements. Import through a SQL client/import facility that supports
the custom delimiter; do not assume a generic semicolon splitter can
execute it. Stop the game server before modifying tables/procedures,
and restore only from a verified backup if needed.

**Never issue blind `DELETE FROM schema_migrations WHERE version=12`,
`DROP TABLE loadtest_jobs`, or `DROP TABLE tdstudio_projects`.**
This can destroy the migration history or saved work.

Only migration 013 is required for `/load start 5 120 full`.
Migration 014 is required only if the external `/loadnet` Windows
Worker is going to be used.
