-- JOB-CLEANER-05. Apply ONCE after schema v9, before deploying the new AMX.
-- Retains the old bin 1 quantity and creates initial default bag positions.
CREATE TABLE cleaner_containers (
  container_id SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  x FLOAT NOT NULL,
  y FLOAT NOT NULL,
  z FLOAT NOT NULL,
  rz FLOAT NOT NULL DEFAULT 90,
  world_id INT NOT NULL DEFAULT 0,
  interior_id INT NOT NULL DEFAULT 0,
  enabled TINYINT(1) NOT NULL DEFAULT 1,
  bin_units TINYINT UNSIGNED NOT NULL DEFAULT 0,
  overflow_units TINYINT UNSIGNED NOT NULL DEFAULT 0,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  CONSTRAINT chk_cc_bin CHECK (bin_units BETWEEN 0 AND 60),
  CONSTRAINT chk_cc_overflow CHECK (overflow_units BETWEEN 0 AND 40),
  CONSTRAINT chk_cc_full CHECK (overflow_units=0 OR bin_units=60)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE cleaner_container_bag_positions (
  container_id SMALLINT UNSIGNED NOT NULL,
  slot_id TINYINT UNSIGNED NOT NULL,
  x FLOAT NOT NULL,
  y FLOAT NOT NULL,
  z FLOAT NOT NULL,
  rz FLOAT NOT NULL DEFAULT 0,
  PRIMARY KEY (container_id,slot_id),
  CONSTRAINT fk_cc_bags FOREIGN KEY (container_id)
    REFERENCES cleaner_containers(container_id) ON DELETE CASCADE,
  CONSTRAINT chk_cc_bag_slot CHECK (slot_id < 10)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Preserve current quantities; move the old Fort Carson installation unchanged.
INSERT INTO cleaner_containers
  (container_id,x,y,z,rz,world_id,interior_id,enabled,bin_units,overflow_units)
SELECT container_id,-227.1175,1066.0662,19.7422,90,0,0,1,bin_units,overflow_units
FROM cleaner_container WHERE container_id=1;

INSERT INTO cleaner_container_bag_positions(container_id,slot_id,x,y,z,rz) VALUES
(1,0,-229.6175,1066.5662,18.8722,0),
(1,1,-230.4175,1066.6662,18.8722,31),
(1,2,-229.6175,1065.4662,18.8722,62),
(1,3,-230.4175,1065.4662,18.8722,93),
(1,4,-231.2175,1066.5662,18.8722,124),
(1,5,-231.2175,1065.4662,18.8722,155),
(1,6,-229.6175,1064.4662,18.8722,186),
(1,7,-230.4175,1064.4662,18.8722,217),
(1,8,-231.2175,1064.4662,18.8722,248),
(1,9,-232.0175,1065.4662,18.8722,279);

-- Legacy table intentionally retained for rollback; it is no longer written.
INSERT INTO schema_migrations(version) VALUES (10);
