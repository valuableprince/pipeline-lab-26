-- CRIME-01: three criminal syndicates without reassigning existing faction IDs.
-- Apply once after migration 030, with the game server stopped.
-- 1-3: legacy civil organizations; 4: reserved; 5: Ministry of Finance.
-- 6-8: criminal organizations. Rank model enforced by Leader_MaxRank in Pawn.
START TRANSACTION;
INSERT INTO factions (faction_id, is_enabled, term_start, term_end) VALUES
    (6, 1, 0, 0),
    (7, 1, 0, 0),
    (8, 1, 0, 0);
INSERT INTO schema_migrations (version) VALUES (31);
COMMIT;
