-- TextDraw Studio format 1. Apply once after 011, before the stage-2 AMX.
-- MySQL 8.0.16+, InnoDB. Typed routines provide one atomic asynchronous CALL.
-- Game CP1251 bytes are transported as HEX; no connection charset assumptions.
CREATE TABLE tdstudio_projects (
  project_id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  name VARCHAR(32) CHARACTER SET cp1251 COLLATE cp1251_bin NOT NULL,
  description VARCHAR(128) CHARACTER SET cp1251 COLLATE cp1251_bin NOT NULL DEFAULT '',
  creator_id BIGINT UNSIGNED NOT NULL,
  interface_type TINYINT UNSIGNED NOT NULL,
  format_version SMALLINT UNSIGNED NOT NULL DEFAULT 1,
  revision INT UNSIGNED NOT NULL DEFAULT 1,
  next_element_id INT UNSIGNED NOT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_tds_owner (creator_id, project_id),
  CONSTRAINT fk_tds_creator FOREIGN KEY (creator_id) REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_tds_project CHECK (interface_type IN (0,1)
    AND revision BETWEEN 1 AND 2147483646 AND next_element_id BETWEEN 1 AND 2147483646)
) ENGINE=InnoDB;

CREATE TABLE tdstudio_elements (
  project_id INT UNSIGNED NOT NULL,
  ordinal TINYINT UNSIGNED NOT NULL,
  `element_id` INT NOT NULL,
  `draw_order` INT NOT NULL,
  `kind` INT NOT NULL,
  `x` DOUBLE NOT NULL,
  `y` DOUBLE NOT NULL,
  `width` DOUBLE NOT NULL,
  `height` DOUBLE NOT NULL,
  `letter_x` DOUBLE NOT NULL,
  `letter_y` DOUBLE NOT NULL,
  `colour` INT NOT NULL,
  `box_colour` INT NOT NULL,
  `background` INT NOT NULL,
  `font` INT NOT NULL,
  `alignment` INT NOT NULL,
  `outline` INT NOT NULL,
  `shadow` INT NOT NULL,
  `use_box` INT NOT NULL,
  `proportional` INT NOT NULL,
  `selectable` INT NOT NULL,
  `sprite` INT NOT NULL,
  `model` INT NOT NULL,
  `rot_x` DOUBLE NOT NULL,
  `rot_y` DOUBLE NOT NULL,
  `rot_z` DOUBLE NOT NULL,
  `zoom` DOUBLE NOT NULL,
  `vehicle_colour1` INT NOT NULL,
  `vehicle_colour2` INT NOT NULL,
  content VARCHAR(127) CHARACTER SET cp1251 COLLATE cp1251_bin NOT NULL,
  PRIMARY KEY (project_id, element_id),
  UNIQUE KEY uq_tds_ordinal (project_id, ordinal),
  UNIQUE KEY uq_tds_order (project_id, draw_order),
  CONSTRAINT fk_tds_project FOREIGN KEY (project_id) REFERENCES tdstudio_projects(project_id) ON DELETE CASCADE,
  CONSTRAINT chk_tds_element CHECK (ordinal < 64 AND element_id BETWEEN 1 AND 2147483645 AND draw_order >= 0
    AND kind BETWEEN 0 AND 3 AND x BETWEEN 0 AND 640 AND y BETWEEN 0 AND 448
    AND width BETWEEN 0.1 AND 640 AND height BETWEEN 0.1 AND 448
    AND letter_x BETWEEN 0.01 AND 5 AND letter_y BETWEEN 0.01 AND 10
    AND font BETWEEN 0 AND 3 AND alignment BETWEEN 1 AND 3
    AND outline BETWEEN 0 AND 4 AND shadow BETWEEN 0 AND 4
    AND use_box IN (0,1) AND proportional IN (0,1) AND selectable IN (0,1)
    AND sprite BETWEEN 0 AND 5 AND model IN (411,560,26,2852,1220,18633)
    AND rot_x BETWEEN -360 AND 360 AND rot_y BETWEEN -360 AND 360 AND rot_z BETWEEN -360 AND 360
    AND zoom BETWEEN 0.1 AND 10 AND vehicle_colour1 BETWEEN 0 AND 255 AND vehicle_colour2 BETWEEN 0 AND 255)
) ENGINE=InnoDB;

DELIMITER $$
CREATE PROCEDURE tds_save(
  IN p_id INT, IN p_actor BIGINT UNSIGNED, IN p_owner_priv BOOLEAN, IN p_revision INT,
  IN p_name VARBINARY(32), IN p_description VARBINARY(128), IN p_type TINYINT,
  IN p_next INT, IN p_rows LONGTEXT CHARACTER SET ascii)
SQL SECURITY INVOKER
proc: BEGIN
  DECLARE v_id INT DEFAULT p_id;
  DECLARE v_owner BIGINT UNSIGNED DEFAULT NULL;
  DECLARE v_revision INT DEFAULT NULL;
  DECLARE v_format INT DEFAULT NULL;
  DECLARE v_count INT DEFAULT 0;
  DECLARE i INT DEFAULT 0;
  DECLARE j INT DEFAULT 0;
  DECLARE v_hex TEXT;
  DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN
    ROLLBACK;
    SELECT 0 AS ok;
  END;
  IF p_id < 0 OR p_actor = 0 OR p_type NOT IN (0,1) OR p_next NOT BETWEEN 1 AND 2147483646
    OR OCTET_LENGTH(p_name) NOT BETWEEN 1 AND 32 OR NOT JSON_VALID(p_rows)
    OR JSON_TYPE(p_rows) <> 'ARRAY' OR JSON_LENGTH(p_rows) > 64 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid TextDraw project';
  END IF;
  START TRANSACTION;
  IF p_id = 0 THEN
    SELECT COUNT(*) INTO v_count FROM tdstudio_projects WHERE creator_id=p_actor FOR UPDATE;
    IF v_count >= 100 THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Project quota'; END IF;
    INSERT INTO tdstudio_projects (name,description,creator_id,interface_type,next_element_id,created_at,updated_at)
      VALUES (CONVERT(p_name USING cp1251),CONVERT(p_description USING cp1251),p_actor,p_type,p_next,UTC_TIMESTAMP(),UTC_TIMESTAMP());
    IF LAST_INSERT_ID()>2147483646 THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Project ID exhausted'; END IF;
    SET v_id=LAST_INSERT_ID();
  ELSE
    SELECT creator_id,revision,format_version INTO v_owner,v_revision,v_format
      FROM tdstudio_projects WHERE project_id=p_id FOR UPDATE;
    IF v_owner IS NULL OR (v_owner<>p_actor AND NOT p_owner_priv) OR v_revision<>p_revision
      OR v_format<>1 OR v_revision>=2147483646 THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Access, format or revision conflict';
    END IF;
    UPDATE tdstudio_projects SET name=CONVERT(p_name USING cp1251),description=CONVERT(p_description USING cp1251),
      interface_type=p_type,next_element_id=p_next,revision=revision+1,updated_at=UTC_TIMESTAMP() WHERE project_id=p_id;
    DELETE FROM tdstudio_elements WHERE project_id=p_id;
  END IF;
  WHILE i < JSON_LENGTH(p_rows) DO
    IF JSON_TYPE(JSON_EXTRACT(p_rows,CONCAT('$[',i,']'))) <> 'ARRAY' OR JSON_LENGTH(JSON_EXTRACT(p_rows,CONCAT('$[',i,']'))) <> 28 THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid element shape'; END IF;
    SET j=0;
    WHILE j < 27 DO
      IF JSON_TYPE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][',j,']'))) NOT IN ('INTEGER','DOUBLE','DECIMAL') THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid element scalar';
      END IF;
      IF j NOT IN (3,4,5,6,7,8,21,22,23,24) AND JSON_TYPE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][',j,']'))) <> 'INTEGER' THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Fractional integer property';
      END IF;
      SET j=j+1;
    END WHILE;
    IF JSON_TYPE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][27]'))) <> 'STRING' THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid text type'; END IF;
    SET v_hex=JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][27]')));
    IF v_hex IS NULL OR NOT REGEXP_LIKE(v_hex,'^([0-9A-Fa-f]{2}){1,127}$') THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid text'; END IF;
    IF CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][0]'))) AS SIGNED) >= p_next THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid next id'; END IF;
    INSERT INTO tdstudio_elements (project_id,ordinal,`element_id`,`draw_order`,`kind`,`x`,`y`,`width`,`height`,`letter_x`,`letter_y`,`colour`,`box_colour`,`background`,`font`,`alignment`,`outline`,`shadow`,`use_box`,`proportional`,`selectable`,`sprite`,`model`,`rot_x`,`rot_y`,`rot_z`,`zoom`,`vehicle_colour1`,`vehicle_colour2`,content)
      VALUES (v_id,i,
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][0]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][1]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][2]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][3]'))) AS DECIMAL(20,9)),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][4]'))) AS DECIMAL(20,9)),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][5]'))) AS DECIMAL(20,9)),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][6]'))) AS DECIMAL(20,9)),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][7]'))) AS DECIMAL(20,9)),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][8]'))) AS DECIMAL(20,9)),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][9]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][10]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][11]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][12]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][13]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][14]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][15]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][16]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][17]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][18]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][19]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][20]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][21]'))) AS DECIMAL(20,9)),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][22]'))) AS DECIMAL(20,9)),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][23]'))) AS DECIMAL(20,9)),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][24]'))) AS DECIMAL(20,9)),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][25]'))) AS SIGNED),
        CAST(JSON_UNQUOTE(JSON_EXTRACT(p_rows,CONCAT('$[',i,'][26]'))) AS SIGNED),
        CONVERT(UNHEX(v_hex) USING cp1251));
    SET i=i+1;
  END WHILE;
  COMMIT;
  SELECT 1 AS ok,project_id,creator_id,revision,
    DATE_FORMAT(created_at,'%Y-%m-%d %H:%i:%s') AS created_at,
    DATE_FORMAT(updated_at,'%Y-%m-%d %H:%i:%s') AS updated_at
    FROM tdstudio_projects WHERE project_id=v_id;
END$$

CREATE PROCEDURE tds_delete(IN p_id INT, IN p_actor BIGINT UNSIGNED, IN p_owner_priv BOOLEAN, IN p_revision INT)
SQL SECURITY INVOKER
proc: BEGIN
  DECLARE v_owner BIGINT UNSIGNED DEFAULT NULL;
  DECLARE v_revision INT DEFAULT NULL;
  DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; SELECT 0 AS ok; END;
  START TRANSACTION;
  SELECT creator_id,revision INTO v_owner,v_revision FROM tdstudio_projects WHERE project_id=p_id FOR UPDATE;
  IF v_owner IS NULL OR (v_owner<>p_actor AND NOT p_owner_priv) OR v_revision<>p_revision THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Access or revision conflict';
  END IF;
  DELETE FROM tdstudio_projects WHERE project_id=p_id;
  COMMIT;
  SELECT 1 AS ok;
END$$

CREATE PROCEDURE tds_list(IN p_actor BIGINT UNSIGNED, IN p_owner_priv BOOLEAN, IN p_offset INT)
SQL SECURITY INVOKER
BEGIN
  IF p_offset < 0 OR p_offset > 1000000 THEN SIGNAL SQLSTATE '45000'; END IF;
  SELECT project_id,HEX(name) AS name_hex,creator_id FROM tdstudio_projects
    WHERE creator_id=p_actor OR p_owner_priv ORDER BY project_id DESC LIMIT p_offset,21;
END$$

CREATE PROCEDURE tds_load(IN p_id INT, IN p_actor BIGINT UNSIGNED, IN p_owner_priv BOOLEAN)
SQL SECURITY INVOKER
BEGIN
  SELECT p.project_id,HEX(p.name) AS name_hex,HEX(p.description) AS description_hex,
    p.creator_id,p.interface_type,p.format_version,p.revision,p.next_element_id,
    DATE_FORMAT(p.created_at,'%Y-%m-%d %H:%i:%s') AS created_at,
    DATE_FORMAT(p.updated_at,'%Y-%m-%d %H:%i:%s') AS updated_at,
    (SELECT COUNT(*) FROM tdstudio_elements c WHERE c.project_id=p.project_id) AS element_count,
    e.`element_id`,e.`draw_order`,e.`kind`,e.`x`,e.`y`,e.`width`,e.`height`,e.`letter_x`,e.`letter_y`,e.`colour`,e.`box_colour`,e.`background`,e.`font`,e.`alignment`,e.`outline`,e.`shadow`,e.`use_box`,e.`proportional`,e.`selectable`,e.`sprite`,e.`model`,e.`rot_x`,e.`rot_y`,e.`rot_z`,e.`zoom`,e.`vehicle_colour1`,e.`vehicle_colour2`,e.ordinal,HEX(e.content) AS content_hex
    FROM tdstudio_projects p LEFT JOIN tdstudio_elements e ON e.project_id=p.project_id
    WHERE p.project_id=p_id AND (p.creator_id=p_actor OR p_owner_priv) ORDER BY e.ordinal;
END$$
DELIMITER ;
INSERT INTO schema_migrations (version) VALUES (12);