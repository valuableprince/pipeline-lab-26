-- CRIME-02.5 | physical evidence interaction and witnesses.
-- Apply once after 035, with stopped server / verified DB backup.
-- One evidence discovery per accepted scenario, durable across relog.
ALTER TABLE crime_contracts
  ADD COLUMN evidence_found_at INT UNSIGNED NOT NULL DEFAULT 0;

-- Existing scenario missions on second stage must remain finishable.
-- Earlier stages already recorded CLUE_FOUND; backfill that historical proof.
UPDATE crime_contracts c
JOIN crime_contract_events e ON e.contract_id=c.contract_id
  AND e.event_type='CLUE_FOUND'
SET c.evidence_found_at=GREATEST(1,c.picked_up_at)
WHERE c.scenario_kind BETWEEN 1 AND 3
  AND c.state IN (2,3)
  AND c.evidence_found_at=0;

-- World interactions are indexed by unique (contract_id,event_type) from 032.
-- An evidence discovery is stored transactionally with its one-time event.
INSERT INTO schema_migrations (version) VALUES (36);
