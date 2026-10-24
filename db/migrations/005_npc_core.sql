-- TARKENSO / NPC-CORE-01 / schema v5 / MariaDB 10.11
-- Execute once in phpMyAdmin on s28_db1 AFTER schema 004.
-- Existing route rows/points are retained; widen ONLY the allowed slot range.
-- MariaDB 10.11 supports DROP CONSTRAINT for named CHECK constraints.
-- v4 game mode remains usable until the new AMX is installed.
START TRANSACTION;
-- Keep route IDs 0..3 unchanged and permit new slots 4..15.
ALTER TABLE npc_routes DROP CONSTRAINT chk_npc_route_slot;
ALTER TABLE npc_routes ADD CONSTRAINT chk_npc_route_slot CHECK (route_slot BETWEEN 0 AND 15);
CREATE TABLE npc_skins (
  skin_id SMALLINT UNSIGNED NOT NULL PRIMARY KEY,
  gender ENUM('male','female') NOT NULL,
  is_active BOOLEAN NOT NULL DEFAULT TRUE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE npc_names (
  name_id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  display_name VARCHAR(24) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,
  gender ENUM('male','female') NOT NULL,
  is_active BOOLEAN NOT NULL DEFAULT TRUE,
  UNIQUE KEY uq_npc_name_gender (display_name, gender),
  CONSTRAINT chk_npc_name_chars CHECK (display_name REGEXP '^[A-Za-z][A-Za-z_]{1,23}$')
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE npcs (
  npc_id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  display_name VARCHAR(24) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,
  skin_id SMALLINT UNSIGNED NOT NULL,
  behavior TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=idle, 1=route',
  route_slot TINYINT UNSIGNED NULL,
  pos_x DECIMAL(10,4) NOT NULL DEFAULT 0,
  pos_y DECIMAL(10,4) NOT NULL DEFAULT 0,
  pos_z DECIMAL(10,4) NOT NULL DEFAULT 0,
  facing DECIMAL(7,3) NOT NULL DEFAULT 0,
  world_id INT UNSIGNED NOT NULL DEFAULT 0,
  interior_id TINYINT UNSIGNED NOT NULL DEFAULT 0,
  is_enabled BOOLEAN NOT NULL DEFAULT FALSE,
  row_version BIGINT UNSIGNED NOT NULL DEFAULT 0,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  UNIQUE KEY uq_npc_display_name (display_name),
  INDEX ix_npc_enabled (is_enabled, npc_id),
  CONSTRAINT fk_npc_skin FOREIGN KEY (skin_id) REFERENCES npc_skins (skin_id),
  CONSTRAINT fk_npc_route_slot FOREIGN KEY (route_slot) REFERENCES npc_routes (route_slot) ON DELETE RESTRICT,
  CONSTRAINT chk_npc_behavior CHECK (behavior IN (0,1)),
  CONSTRAINT chk_npc_mode_route CHECK ((behavior=0 AND route_slot IS NULL) OR (behavior=1 AND route_slot IS NOT NULL)),
  CONSTRAINT chk_npc_coords CHECK (pos_x BETWEEN -3000 AND 3000 AND pos_y BETWEEN -3000 AND 3000 AND pos_z BETWEEN -150 AND 2000),
  CONSTRAINT chk_npc_heading CHECK (facing >= 0 AND facing <= 360),
  CONSTRAINT chk_npc_world CHECK (world_id <= 100000 AND interior_id <= 255),
  CONSTRAINT chk_npc_name CHECK (display_name REGEXP '^[A-Za-z][A-Za-z_0-9]{1,23}$')
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- First verified examples only; expand this table from an audited skin catalogue.
-- An unknown skin ID must be rejected rather than guessed as male/female.
INSERT INTO npc_skins (skin_id,gender) VALUES
  (0,'male'),(1,'male'),(2,'male'),(7,'male'),(15,'male'),
  (60,'male'),(61,'male'),(9,'female'),(10,'female'),
  (11,'female'),(12,'female'),(13,'female'),(31,'female');

INSERT INTO npc_names (display_name,gender) VALUES
  ('Alexander','male'),('Michael','male'),('Daniel','male'),
  ('Victor','male'),('Nathan','male'),('Thomas','male'),
  ('Robert','male'),('David','male'),('Leon','male'),
  ('Jessica','female'),('Emily','female'),('Olivia','female'),
  ('Sophia','female'),('Hannah','female'),('Claire','female'),
  ('Victoria','female'),('Maria','female'),('Anna','female');

INSERT INTO schema_migrations (version) VALUES (5);
COMMIT;
