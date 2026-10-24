-- TARKENSO: upgrade a NEW empty-on-data s28_db1 installed from 001_initial.sql (v1)
-- Run ONCE in phpMyAdmin SQL for the s28_db1 database. DO NOT repeat.
-- v2: account spawn region
ALTER TABLE accounts
  ADD COLUMN start_region TINYINT UNSIGNED NOT NULL DEFAULT 1,
  ADD CONSTRAINT chk_accounts_start_region CHECK (start_region BETWEEN 1 AND 3);
INSERT INTO schema_migrations (version) VALUES (2);

-- v3: persistent world, organisations, routes and editor state
CREATE TABLE studio_zones (
  slot_id TINYINT UNSIGNED NOT NULL PRIMARY KEY,
  is_active BOOLEAN NOT NULL DEFAULT FALSE,
  x DECIMAL(10,4) NOT NULL,
  y DECIMAL(10,4) NOT NULL,
  z DECIMAL(10,4) NOT NULL,
  radius DECIMAL(7,3) NOT NULL,
  world_id INT UNSIGNED NOT NULL DEFAULT 0,
  interior_id TINYINT UNSIGNED NOT NULL DEFAULT 0,
  zone_mode TINYINT UNSIGNED NOT NULL DEFAULT 0,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  CONSTRAINT chk_studio_slot CHECK (slot_id BETWEEN 0 AND 15),
  CONSTRAINT chk_studio_radius CHECK (radius BETWEEN 1 AND 50),
  CONSTRAINT chk_studio_mode CHECK (zone_mode BETWEEN 0 AND 1),
  CONSTRAINT chk_studio_world CHECK (world_id <= 100000),
  CONSTRAINT chk_studio_position CHECK (x BETWEEN -3000 AND 3000 AND
    y BETWEEN -3000 AND 3000 AND z BETWEEN -150 AND 2000)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE npc_routes (
  route_slot TINYINT UNSIGNED NOT NULL PRIMARY KEY,
  is_enabled BOOLEAN NOT NULL DEFAULT TRUE,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  CONSTRAINT chk_npc_route_slot CHECK (route_slot BETWEEN 0 AND 3)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE npc_route_points (
  route_slot TINYINT UNSIGNED NOT NULL,
  point_index TINYINT UNSIGNED NOT NULL,
  x DECIMAL(10,4) NOT NULL,
  y DECIMAL(10,4) NOT NULL,
  z DECIMAL(10,4) NOT NULL,
  cross_edge BOOLEAN NOT NULL DEFAULT FALSE,
  PRIMARY KEY (route_slot, point_index),
  CONSTRAINT fk_npc_route FOREIGN KEY (route_slot) REFERENCES npc_routes(route_slot) ON DELETE CASCADE,
  CONSTRAINT chk_npc_point_index CHECK (point_index <= 127),
  CONSTRAINT chk_npc_coordinates CHECK (
    x BETWEEN -3000 AND 3000 AND y BETWEEN -3000 AND 3000 AND z BETWEEN -150 AND 2000)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO factions (faction_id,is_enabled,term_start,term_end) VALUES
  (1,1,0,0),(2,1,0,0),(3,1,0,0),(4,0,0,0)
ON DUPLICATE KEY UPDATE faction_id=VALUES(faction_id);

INSERT INTO cleaner_districts (district_id,pollution) VALUES (0,20),(1,20)
ON DUPLICATE KEY UPDATE district_id=VALUES(district_id);

INSERT INTO schema_migrations (version) VALUES (3);
