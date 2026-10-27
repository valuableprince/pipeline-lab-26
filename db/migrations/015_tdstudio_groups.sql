-- TextDraw Studio format 2; 013/014 are already reserved in current main.
-- Apply after 012. Does not modify/drop existing projects or v1 routines.
ALTER TABLE tdstudio_projects ADD COLUMN next_group_id INT UNSIGNED NOT NULL DEFAULT 1,
  ADD CONSTRAINT chk_tds_next_group CHECK (next_group_id BETWEEN 1 AND 2147483646);

CREATE TABLE tdstudio_groups (
  project_id INT UNSIGNED NOT NULL,
  group_id INT NOT NULL,
  name VARCHAR(32) CHARACTER SET cp1251 COLLATE cp1251_bin NOT NULL,
  PRIMARY KEY(project_id,group_id),
  UNIQUE KEY uq_tds_group_name(project_id,name),
  CONSTRAINT fk_tds_group_project FOREIGN KEY(project_id) REFERENCES tdstudio_projects(project_id) ON DELETE CASCADE,
  CONSTRAINT chk_tds_group CHECK (group_id BETWEEN 1 AND 2147483645 AND OCTET_LENGTH(name) BETWEEN 1 AND 32)
) ENGINE=InnoDB;

CREATE TABLE tdstudio_group_members (
  project_id INT UNSIGNED NOT NULL,
  element_id INT NOT NULL,
  group_id INT NOT NULL,
  PRIMARY KEY(project_id,element_id),
  KEY idx_tds_members_group(project_id,group_id),
  CONSTRAINT fk_tds_member_element FOREIGN KEY(project_id,element_id) REFERENCES tdstudio_elements(project_id,element_id) ON DELETE CASCADE,
  CONSTRAINT fk_tds_member_group FOREIGN KEY(project_id,group_id) REFERENCES tdstudio_groups(project_id,group_id) ON DELETE CASCADE
) ENGINE=InnoDB;

DELIMITER $$
CREATE PROCEDURE tds_save_v2(
  IN p_id INT, IN p_actor BIGINT UNSIGNED, IN p_owner_priv BOOLEAN, IN p_revision INT,
  IN p_name VARBINARY(32), IN p_description VARBINARY(128), IN p_type TINYINT,
  IN p_next INT, IN p_rows LONGTEXT CHARACTER SET ascii,
  IN p_next_group INT, IN p_groups LONGTEXT CHARACTER SET ascii)
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
  DECLARE v_group INT;
  DECLARE v_member INT;
  DECLARE v_old_next INT;
  DECLARE v_old_group INT;
  DECLARE v_members JSON;
  DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN
    ROLLBACK;
    SELECT 0 AS ok;
  END;
  IF p_id IS NULL OR p_actor IS NULL OR p_owner_priv IS NULL OR p_owner_priv NOT IN (0,1)
    OR p_rows IS NULL OR p_groups IS NULL OR p_next_group IS NULL
    OR p_next_group NOT BETWEEN 1 AND 2147483646
    OR NOT JSON_VALID(p_groups) OR JSON_TYPE(p_groups) <> 'ARRAY' OR JSON_LENGTH(p_groups) > 16
    OR p_id < 0 OR p_actor = 0 OR p_type NOT IN (0,1) OR p_next NOT BETWEEN 1 AND 2147483646
    OR OCTET_LENGTH(p_name) NOT BETWEEN 1 AND 32 OR NOT JSON_VALID(p_rows)
    OR JSON_TYPE(p_rows) <> 'ARRAY' OR JSON_LENGTH(p_rows) > 64 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid TextDraw project';
  END IF;
  START TRANSACTION;
  IF p_id = 0 THEN
    SELECT COUNT(*) INTO v_count FROM tdstudio_projects WHERE creator_id=p_actor FOR UPDATE;
    IF v_count >= 100 THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Project quota'; END IF;
    INSERT INTO tdstudio_projects (name,description,creator_id,interface_type,next_element_id,format_version,next_group_id,created_at,updated_at)
      VALUES (CONVERT(p_name USING cp1251),CONVERT(p_description USING cp1251),p_actor,p_type,p_next,2,p_next_group,UTC_TIMESTAMP(),UTC_TIMESTAMP());
    IF LAST_INSERT_ID()>2147483646 THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Project ID exhausted'; END IF;
    SET v_id=LAST_INSERT_ID();
  ELSE
    SELECT creator_id,revision,format_version,next_element_id,next_group_id INTO v_owner,v_revision,v_format,v_old_next,v_old_group
      FROM tdstudio_projects WHERE project_id=p_id FOR UPDATE;
    IF v_owner IS NULL OR (v_owner<>p_actor AND NOT p_owner_priv) OR v_revision<>p_revision
      OR v_format NOT IN (1,2) OR v_revision>=2147483646 OR p_next<v_old_next OR p_next_group<v_old_group THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Access, format or revision conflict';
    END IF;
    UPDATE tdstudio_projects SET name=CONVERT(p_name USING cp1251),description=CONVERT(p_description USING cp1251),
      interface_type=p_type,next_element_id=p_next,format_version=2,next_group_id=p_next_group,revision=revision+1,updated_at=UTC_TIMESTAMP() WHERE project_id=p_id;
    DELETE FROM tdstudio_groups WHERE project_id=p_id;
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
    INSERT INTO tdstudio_groups(project_id,group_id,name) VALUES(v_id,v_group,CONVERT(UNHEX(v_hex) USING cp1251));
    SET j=0;
    WHILE j < JSON_LENGTH(v_members) DO
      IF JSON_TYPE(JSON_EXTRACT(v_members,CONCAT('$[',j,']'))) <> 'INTEGER' THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Invalid group member type';
      END IF;
      SET v_member=CAST(JSON_UNQUOTE(JSON_EXTRACT(v_members,CONCAT('$[',j,']'))) AS SIGNED);
      INSERT INTO tdstudio_group_members(project_id,element_id,group_id) VALUES(v_id,v_member,v_group);
      SET j=j+1;
    END WHILE;
    SET i=i+1;
  END WHILE;
  COMMIT;
  SELECT 1 AS ok,project_id,creator_id,revision,
    DATE_FORMAT(created_at,'%Y-%m-%d %H:%i:%s') AS created_at,
    DATE_FORMAT(updated_at,'%Y-%m-%d %H:%i:%s') AS updated_at
    FROM tdstudio_projects WHERE project_id=v_id;
END$$

CREATE PROCEDURE tds_load_v2(IN p_id INT, IN p_actor BIGINT UNSIGNED, IN p_owner_priv BOOLEAN)
SQL SECURITY INVOKER
BEGIN
  SELECT p.project_id,HEX(p.name) AS name_hex,HEX(p.description) AS description_hex,
    p.creator_id,p.interface_type,p.format_version,p.revision,p.next_element_id,p.next_group_id,
    (SELECT COUNT(*) FROM tdstudio_groups g WHERE g.project_id=p.project_id) AS group_count,
    COALESCE(m.group_id,0) AS group_id,COALESCE(HEX(g.name),'') AS group_name_hex,
    DATE_FORMAT(p.created_at,'%Y-%m-%d %H:%i:%s') AS created_at,
    DATE_FORMAT(p.updated_at,'%Y-%m-%d %H:%i:%s') AS updated_at,
    (SELECT COUNT(*) FROM tdstudio_elements c WHERE c.project_id=p.project_id) AS element_count,
    e.`element_id`,e.`draw_order`,e.`kind`,e.`x`,e.`y`,e.`width`,e.`height`,e.`letter_x`,e.`letter_y`,e.`colour`,e.`box_colour`,e.`background`,e.`font`,e.`alignment`,e.`outline`,e.`shadow`,e.`use_box`,e.`proportional`,e.`selectable`,e.`sprite`,e.`model`,e.`rot_x`,e.`rot_y`,e.`rot_z`,e.`zoom`,e.`vehicle_colour1`,e.`vehicle_colour2`,e.ordinal,HEX(e.content) AS content_hex
    FROM tdstudio_projects p LEFT JOIN tdstudio_elements e ON e.project_id=p.project_id
    LEFT JOIN tdstudio_group_members m ON m.project_id=e.project_id AND m.element_id=e.element_id
    LEFT JOIN tdstudio_groups g ON g.project_id=m.project_id AND g.group_id=m.group_id
    WHERE p.project_id=p_id AND (p.creator_id=p_actor OR p_owner_priv) ORDER BY e.ordinal;
END$$
DELIMITER ;
INSERT INTO schema_migrations(version) VALUES(15);
