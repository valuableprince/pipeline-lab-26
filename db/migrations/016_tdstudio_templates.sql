-- Stage 3.3. 013/014 are reserved in main, 015 contains editor groups.
-- Apply after 015. Existing projects/routines are never modified.
CREATE TABLE tdstudio_templates (
  template_id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  name VARCHAR(32) CHARACTER SET cp1251 COLLATE cp1251_bin NOT NULL,
  description VARCHAR(128) CHARACTER SET cp1251 COLLATE cp1251_bin NOT NULL DEFAULT '',
  creator_id BIGINT UNSIGNED NOT NULL,
  category SMALLINT UNSIGNED NOT NULL,
  visibility TINYINT UNSIGNED NOT NULL DEFAULT 0,
  interface_type TINYINT UNSIGNED NOT NULL,
  format_version SMALLINT UNSIGNED NOT NULL DEFAULT 2,
  revision INT UNSIGNED NOT NULL DEFAULT 1,
  next_element_id INT UNSIGNED NOT NULL,
  next_group_id INT UNSIGNED NOT NULL,
  origin_x DOUBLE NOT NULL,
  origin_y DOUBLE NOT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  KEY idx_tdst_owner (creator_id, category, template_id),
  KEY idx_tdst_public (visibility, category, template_id),
  CONSTRAINT fk_tdst_creator FOREIGN KEY(creator_id) REFERENCES accounts(account_id) ON DELETE RESTRICT,
  CONSTRAINT chk_tdst_meta CHECK(category BETWEEN 1 AND 65535 AND visibility IN (0,1)
    AND format_version=2 AND interface_type IN (0,1) AND revision BETWEEN 1 AND 2147483646
    AND next_element_id BETWEEN 1 AND 2147483646 AND next_group_id BETWEEN 1 AND 2147483646
    AND origin_x BETWEEN -640 AND 640 AND origin_y BETWEEN 0 AND 448)
) ENGINE=InnoDB;

CREATE TABLE tdstudio_template_elements (
  template_id INT UNSIGNED NOT NULL,
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
  PRIMARY KEY (template_id, element_id),
  UNIQUE KEY uq_tdst_ordinal (template_id, ordinal),
  UNIQUE KEY uq_tdst_order (template_id, draw_order),
  CONSTRAINT fk_tdst_template FOREIGN KEY (template_id) REFERENCES tdstudio_templates(template_id) ON DELETE CASCADE,
  CONSTRAINT chk_tdst_element CHECK (ordinal < 64 AND element_id BETWEEN 1 AND 2147483645 AND draw_order >= 0
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

CREATE TABLE tdstudio_template_groups (
  template_id INT UNSIGNED NOT NULL,
  group_id INT NOT NULL,
  name VARCHAR(32) CHARACTER SET cp1251 COLLATE cp1251_bin NOT NULL,
  PRIMARY KEY(template_id,group_id),
  UNIQUE KEY uq_tdst_group_name(template_id,name),
  CONSTRAINT fk_tdst_group_project FOREIGN KEY(template_id) REFERENCES tdstudio_templates(template_id) ON DELETE CASCADE,
  CONSTRAINT chk_tdst_group CHECK (group_id BETWEEN 1 AND 2147483645 AND OCTET_LENGTH(name) BETWEEN 1 AND 32)
) ENGINE=InnoDB;

CREATE TABLE tdstudio_template_members (
  template_id INT UNSIGNED NOT NULL,
  element_id INT NOT NULL,
  group_id INT NOT NULL,
  PRIMARY KEY(template_id,element_id),
  KEY idx_tdst_members_group(template_id,group_id),
  CONSTRAINT fk_tdst_member_element FOREIGN KEY(template_id,element_id) REFERENCES tdstudio_template_elements(template_id,element_id) ON DELETE CASCADE,
  CONSTRAINT fk_tdst_member_group FOREIGN KEY(template_id,group_id) REFERENCES tdstudio_template_groups(template_id,group_id) ON DELETE CASCADE
) ENGINE=InnoDB;

DELIMITER $$
CREATE PROCEDURE tds_template_save(
  IN p_actor BIGINT UNSIGNED, IN p_owner_priv BOOLEAN, IN p_name VARBINARY(32),
  IN p_description VARBINARY(128), IN p_category INT, IN p_visibility INT,
  IN p_type INT, IN p_next INT, IN p_rows LONGTEXT CHARACTER SET ascii,
  IN p_next_group INT, IN p_groups LONGTEXT CHARACTER SET ascii,
  IN p_origin_x DOUBLE, IN p_origin_y DOUBLE)
SQL SECURITY INVOKER
proc: BEGIN
  DECLARE v_id INT;
  DECLARE v_count INT;
  DECLARE v_actor BIGINT UNSIGNED;
  DECLARE i INT DEFAULT 0;
  DECLARE j INT DEFAULT 0;
  DECLARE v_hex TEXT;
  DECLARE v_group INT;
  DECLARE v_member INT;
  DECLARE v_members JSON;
  DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; SELECT 0 AS ok; END;
  IF p_actor IS NULL OR p_actor=0 OR p_owner_priv IS NULL OR p_owner_priv NOT IN (0,1)
    OR p_visibility IS NULL OR p_visibility NOT IN (0,1) OR (p_visibility=1 AND NOT p_owner_priv)
    OR p_category IS NULL OR p_category NOT BETWEEN 1 AND 65535
    OR p_name IS NULL OR OCTET_LENGTH(p_name) NOT BETWEEN 1 AND 32 OR LOCATE(X'00',p_name)>0
    OR p_description IS NULL OR OCTET_LENGTH(p_description)>128 OR LOCATE(X'00',p_description)>0
    OR p_type IS NULL OR p_type NOT IN (0,1) OR p_next IS NULL OR p_next NOT BETWEEN 1 AND 2147483646
    OR p_next_group IS NULL OR p_next_group NOT BETWEEN 1 AND 2147483646
    OR p_origin_x IS NULL OR p_origin_x NOT BETWEEN -640 AND 640
    OR p_origin_y IS NULL OR p_origin_y NOT BETWEEN 0 AND 448
    OR p_rows IS NULL OR OCTET_LENGTH(p_rows)>48000 OR NOT JSON_VALID(p_rows)
    OR JSON_TYPE(p_rows)<>'ARRAY' OR JSON_LENGTH(p_rows) NOT BETWEEN 1 AND 64
    OR p_groups IS NULL OR OCTET_LENGTH(p_groups)>4096 OR NOT JSON_VALID(p_groups)
    OR JSON_TYPE(p_groups)<>'ARRAY' OR JSON_LENGTH(p_groups)>16 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid template';
  END IF;
  START TRANSACTION;
  -- Serialize the per-account quota, including simultaneous editor leases.
  SELECT account_id INTO v_actor FROM accounts WHERE account_id=p_actor FOR UPDATE;
  IF v_actor IS NULL THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Unknown creator'; END IF;
  SELECT COUNT(*) INTO v_count FROM tdstudio_templates WHERE creator_id=p_actor;
  IF v_count>=100 THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Template quota'; END IF;
  INSERT INTO tdstudio_templates(name,description,creator_id,category,visibility,interface_type,
    next_element_id,next_group_id,origin_x,origin_y,created_at,updated_at)
    VALUES(CONVERT(p_name USING cp1251),CONVERT(p_description USING cp1251),p_actor,p_category,p_visibility,p_type,
      p_next,p_next_group,p_origin_x,p_origin_y,UTC_TIMESTAMP(),UTC_TIMESTAMP());
  IF LAST_INSERT_ID()>2147483646 THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Template IDs exhausted'; END IF;
  SET v_id=LAST_INSERT_ID();
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
    INSERT INTO tdstudio_template_elements (template_id,ordinal,`element_id`,`draw_order`,`kind`,`x`,`y`,`width`,`height`,`letter_x`,`letter_y`,`colour`,`box_colour`,`background`,`font`,`alignment`,`outline`,`shadow`,`use_box`,`proportional`,`selectable`,`sprite`,`model`,`rot_x`,`rot_y`,`rot_z`,`zoom`,`vehicle_colour1`,`vehicle_colour2`,content)
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
  -- Elements, groups and membership share the same transaction and revision lock.
  SET i=0;
  WHILE i < JSON_LENGTH(p_groups) DO
    IF JSON_TYPE(JSON_EXTRACT(p_groups,CONCAT('$[',i,']'))) <> 'ARRAY'
      OR JSON_LENGTH(JSON_EXTRACT(p_groups,CONCAT('$[',i,']'))) <> 3
      OR JSON_TYPE(JSON_EXTRACT(p_groups,CONCAT('$[',i,'][0]'))) <> 'INTEGER'
      OR JSON_TYPE(JSON_EXTRACT(p_groups,CONCAT('$[',i,'][1]'))) <> 'STRING'
      OR JSON_TYPE(JSON_EXTRACT(p_groups,CONCAT('$[',i,'][2]'))) <> 'ARRAY' THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid group shape';
    END IF;
    SET v_group=CAST(JSON_UNQUOTE(JSON_EXTRACT(p_groups,CONCAT('$[',i,'][0]'))) AS SIGNED);
    SET v_hex=JSON_UNQUOTE(JSON_EXTRACT(p_groups,CONCAT('$[',i,'][1]')));
    SET v_members=JSON_EXTRACT(p_groups,CONCAT('$[',i,'][2]'));
    IF v_group NOT BETWEEN 1 AND 2147483645 OR v_group>=p_next_group
      OR NOT REGEXP_LIKE(v_hex,'^([0-9A-Fa-f]{2}){1,32}$') OR LOCATE(X'00',UNHEX(v_hex))>0
      OR JSON_LENGTH(v_members) NOT BETWEEN 1 AND 64 THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid group data';
    END IF;
    INSERT INTO tdstudio_template_groups(template_id,group_id,name) VALUES(v_id,v_group,CONVERT(UNHEX(v_hex) USING cp1251));
    SET j=0;
    WHILE j < JSON_LENGTH(v_members) DO
      IF JSON_TYPE(JSON_EXTRACT(v_members,CONCAT('$[',j,']'))) <> 'INTEGER' THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid group member type';
      END IF;
      SET v_member=CAST(JSON_UNQUOTE(JSON_EXTRACT(v_members,CONCAT('$[',j,']'))) AS SIGNED);
      INSERT INTO tdstudio_template_members(template_id,element_id,group_id) VALUES(v_id,v_member,v_group);
      SET j=j+1;
    END WHILE;
    SET i=i+1;
  END WHILE;
  IF ABS((SELECT MIN(CASE WHEN kind=0 AND alignment=2 THEN x-width/2
      WHEN kind=0 AND alignment=3 THEN x-width ELSE x END)
      FROM tdstudio_template_elements WHERE template_id=v_id)-p_origin_x)>0.0001
    OR ABS((SELECT MIN(y) FROM tdstudio_template_elements WHERE template_id=v_id)-p_origin_y)>0.0001 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Template origin mismatch';
  END IF;
  COMMIT;
  SELECT 1 AS ok,template_id,revision FROM tdstudio_templates WHERE template_id=v_id;
END$$

CREATE PROCEDURE tds_template_update(IN p_id INT, IN p_actor BIGINT UNSIGNED, IN p_priv BOOLEAN,
  IN p_revision INT, IN p_name VARBINARY(32), IN p_description VARBINARY(128))
SQL SECURITY INVOKER
BEGIN
  UPDATE tdstudio_templates SET name=CONVERT(p_name USING cp1251),description=CONVERT(p_description USING cp1251),
    revision=revision+1,updated_at=UTC_TIMESTAMP()
    WHERE template_id=p_id AND (creator_id=p_actor OR p_priv=1) AND p_actor>0 AND p_priv IN (0,1)
      AND revision=p_revision AND revision<2147483646 AND OCTET_LENGTH(p_name) BETWEEN 1 AND 32
      AND OCTET_LENGTH(p_description)<=128 AND LOCATE(X'00',p_name)=0 AND LOCATE(X'00',p_description)=0;
  SELECT IF(ROW_COUNT()=1,1,0) AS ok;
END$$

CREATE PROCEDURE tds_template_delete(IN p_id INT, IN p_actor BIGINT UNSIGNED, IN p_priv BOOLEAN, IN p_revision INT)
SQL SECURITY INVOKER
BEGIN
  DELETE FROM tdstudio_templates WHERE template_id=p_id AND revision=p_revision
    AND p_actor>0 AND p_priv IN (0,1) AND (creator_id=p_actor OR p_priv=1);
  SELECT IF(ROW_COUNT()=1,1,0) AS ok;
END$$

CREATE PROCEDURE tds_template_list(IN p_actor BIGINT UNSIGNED, IN p_priv BOOLEAN, IN p_category INT, IN p_offset INT)
SQL SECURITY INVOKER
BEGIN
  IF p_offset BETWEEN 0 AND 1000000 AND p_category BETWEEN 0 AND 65535 AND p_priv IN (0,1) AND p_actor>0 THEN
    SELECT template_id,HEX(name) AS name_hex,visibility,category,
      (SELECT COUNT(*) FROM tdstudio_template_elements e WHERE e.template_id=t.template_id) AS element_count
      FROM tdstudio_templates t WHERE (creator_id=p_actor OR p_priv=1 OR visibility=1)
      AND (p_category=0 OR category=p_category) ORDER BY template_id LIMIT p_offset,21;
  END IF;
END$$

CREATE PROCEDURE tds_template_info(IN p_id INT, IN p_actor BIGINT UNSIGNED, IN p_priv BOOLEAN)
SQL SECURITY INVOKER
BEGIN
  SELECT template_id,HEX(name) AS name_hex,HEX(description) AS description_hex,creator_id,category,visibility,
    revision,format_version,origin_x,origin_y,next_element_id,next_group_id,interface_type,
    DATE_FORMAT(created_at,'%Y-%m-%d %H:%i:%s') AS created_at,
    DATE_FORMAT(updated_at,'%Y-%m-%d %H:%i:%s') AS updated_at
    FROM tdstudio_templates WHERE template_id=p_id AND p_actor>0 AND p_priv IN (0,1)
      AND (creator_id=p_actor OR p_priv=1 OR visibility=1);
END$$

CREATE PROCEDURE tds_template_load(IN p_id INT, IN p_actor BIGINT UNSIGNED, IN p_owner_priv BOOLEAN)
SQL SECURITY INVOKER
BEGIN
  SELECT p.template_id,HEX(p.name) AS name_hex,HEX(p.description) AS description_hex,
    p.creator_id,p.interface_type,p.format_version,p.revision,p.category,p.visibility,p.origin_x,p.origin_y,p.next_element_id,p.next_group_id,
    (SELECT COUNT(*) FROM tdstudio_template_groups g WHERE g.template_id=p.template_id) AS group_count,
    COALESCE(m.group_id,0) AS group_id,COALESCE(HEX(g.name),'') AS group_name_hex,
    DATE_FORMAT(p.created_at,'%Y-%m-%d %H:%i:%s') AS created_at,
    DATE_FORMAT(p.updated_at,'%Y-%m-%d %H:%i:%s') AS updated_at,
    (SELECT COUNT(*) FROM tdstudio_template_elements c WHERE c.template_id=p.template_id) AS element_count,
    e.`element_id`,e.`draw_order`,e.`kind`,e.`x`,e.`y`,e.`width`,e.`height`,e.`letter_x`,e.`letter_y`,e.`colour`,e.`box_colour`,e.`background`,e.`font`,e.`alignment`,e.`outline`,e.`shadow`,e.`use_box`,e.`proportional`,e.`selectable`,e.`sprite`,e.`model`,e.`rot_x`,e.`rot_y`,e.`rot_z`,e.`zoom`,e.`vehicle_colour1`,e.`vehicle_colour2`,e.ordinal,HEX(e.content) AS content_hex
    FROM tdstudio_templates p LEFT JOIN tdstudio_template_elements e ON e.template_id=p.template_id
    LEFT JOIN tdstudio_template_members m ON m.template_id=e.template_id AND m.element_id=e.element_id
    LEFT JOIN tdstudio_template_groups g ON g.template_id=m.template_id AND g.group_id=m.group_id
    WHERE p.template_id=p_id AND (p.creator_id=p_actor OR p_owner_priv=1 OR p.visibility=1) AND p_actor>0 AND p_owner_priv IN (0,1) ORDER BY e.ordinal;
END$$
DELIMITER ;
INSERT INTO schema_migrations(version) VALUES(16);
