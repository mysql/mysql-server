
--source include/no_protocol.inc

#
# SETUP
#

UPDATE performance_schema.setup_instruments
  SET ENABLED='NO' WHERE NAME LIKE "wait/synch/%";

#
# DUMP CRITICAL COLUMNS COLLATIONS
#

SELECT TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, COLLATION_NAME
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_SCHEMA = 'performance_schema'
  AND COLUMN_NAME LIKE '%SCHEMA%'
  ORDER BY TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME;

SELECT TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, COLLATION_NAME
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_SCHEMA = 'performance_schema'
  AND COLUMN_NAME LIKE '%OBJECT_NAME%'
  ORDER BY TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME;

SELECT TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, COLLATION_NAME
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_SCHEMA = 'performance_schema'
  AND COLUMN_NAME LIKE '%COLUMN%'
  ORDER BY TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME;


#
# SCHEMA NAME torture
#

TRUNCATE TABLE performance_schema.events_waits_history_long;
TRUNCATE TABLE performance_schema.events_statements_history_long;

CREATE SCHEMA `BroKen`;
CREATE SCHEMA `BroKén`;
CREATE SCHEMA `bRŎKȩn`;
CREATE SCHEMA `∀p∊ℙ`;

SHOW DATABASES;

use `∀p∊ℙ`;

INSERT IGNORE INTO performance_schema.setup_objects VALUES
  ('TABLE', 'BroKen', '%', 'YES', 'YES'),
  ('TABLE', 'BroKén', '%', 'YES', 'YES'),
  ('TABLE', 'bRŎKȩn', '%', 'YES', 'YES'),
  ('TABLE', '∀p∊ℙ', '%', 'YES', 'YES');

-- echo "---------------------------------------------------------"
SELECT * FROM performance_schema.setup_objects
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_SCHEMA USING 'utf8mb4') COLLATE utf8mb4_bin;
-- echo "---------------------------------------------------------"

CREATE TABLE `∀p∊ℙ`.`torture`(a int);
CREATE TABLE `BroKen`.`torture`(a int);
CREATE TABLE `BroKén`.`torture`(a int);
CREATE TABLE `bRŎKȩn`.`torture`(a int);

BEGIN;

SELECT * from `∀p∊ℙ`.`torture`;
SELECT * from `BroKen`.`torture`;
SELECT * from `BroKén`.`torture`;
SELECT * from `bRŎKȩn`.`torture`;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME, EVENT_NAME
  FROM performance_schema.events_waits_history_long
  WHERE OBJECT_NAME = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME, EVENT_NAME,
    CONVERT(OBJECT_SCHEMA USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, EVENT_NAME, CURRENT_SCHEMA
  FROM performance_schema.events_statements_history_long
  WHERE OBJECT_NAME = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, EVENT_NAME,
    CONVERT(OBJECT_SCHEMA USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
  LOCK_TYPE, LOCK_DURATION, LOCK_STATUS
  FROM performance_schema.metadata_locks
  WHERE OBJECT_NAME = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_SCHEMA USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_lock_waits_summary_by_table
  WHERE OBJECT_NAME = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_SCHEMA USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_io_waits_summary_by_table
  WHERE OBJECT_NAME = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_SCHEMA USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_io_waits_summary_by_index_usage
  WHERE OBJECT_NAME = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_SCHEMA USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_handles
  WHERE OBJECT_NAME = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_SCHEMA USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.objects_summary_global_by_type
  WHERE OBJECT_NAME = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_SCHEMA USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
COMMIT;

DELETE FROM performance_schema.setup_objects
  WHERE OBJECT_SCHEMA IN ('BroKen', 'BroKén', 'bRŎKȩn', '∀p∊ℙ');

DROP SCHEMA `BroKen`;
DROP SCHEMA `BroKén`;
DROP SCHEMA `bRŎKȩn`;
DROP SCHEMA `∀p∊ℙ`;

#
# TABLE NAME torture
#

TRUNCATE TABLE performance_schema.events_waits_history_long;
TRUNCATE TABLE performance_schema.events_statements_history_long;

CREATE SCHEMA `torture`;

use `torture`;

INSERT IGNORE INTO performance_schema.setup_objects VALUES
  ('TABLE', 'torture', 'BroKen', 'YES', 'YES'),
  ('TABLE', 'torture', 'BroKén', 'YES', 'YES'),
  ('TABLE', 'torture', 'bRŎKȩn', 'YES', 'YES'),
  ('TABLE', 'torture', '∀p∊ℙ', 'YES', 'YES');

-- echo "---------------------------------------------------------"
SELECT * FROM performance_schema.setup_objects
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;
-- echo "---------------------------------------------------------"

CREATE TABLE `torture`.`∀p∊ℙ`(a int);
CREATE TABLE `torture`.`BroKen`(a int);
CREATE TABLE `torture`.`BroKén`(a int);
CREATE TABLE `torture`.`bRŎKȩn`(a int);

BEGIN;

SELECT * from `torture`.`∀p∊ℙ`;
SELECT * from `torture`.`BroKen`;
SELECT * from `torture`.`BroKén`;
SELECT * from `torture`.`bRŎKȩn`;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME, EVENT_NAME
  FROM performance_schema.events_waits_history_long
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME, EVENT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, EVENT_NAME, CURRENT_SCHEMA
  FROM performance_schema.events_statements_history_long
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, EVENT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
  LOCK_TYPE, LOCK_DURATION, LOCK_STATUS
  FROM performance_schema.metadata_locks
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_lock_waits_summary_by_table
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_io_waits_summary_by_table
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_io_waits_summary_by_index_usage
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_handles
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.objects_summary_global_by_type
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
COMMIT;

DELETE FROM performance_schema.setup_objects
  WHERE OBJECT_NAME IN ('BroKen', 'BroKén', 'bRŎKȩn', '∀p∊ℙ');

DROP TABLE `torture`.`BroKen`;
DROP TABLE `torture`.`BroKén`;
DROP TABLE `torture`.`bRŎKȩn`;
DROP TABLE `torture`.`∀p∊ℙ`;

#
# INDEX NAME torture
#

TRUNCATE TABLE performance_schema.events_waits_history_long;
TRUNCATE TABLE performance_schema.events_statements_history_long;

CREATE TABLE `torture`.`torture`(
  id int,
  `∀p∊ℙ` int, `BroKen` int, `BroKén` int, `bRŎKȩn` int,
  PRIMARY KEY (`id`),
  KEY (`∀p∊ℙ`),
  KEY (`BroKen`),
  KEY (`BroKén`),
  KEY (`bRŎKȩn`)
);

BEGIN;

INSERT INTO `torture`.`torture` VALUES (0, 1, 2, 3, 4);

SELECT * from `torture`.`torture`;

SELECT * from `torture`.`torture` USE INDEX (`∀p∊ℙ`) WHERE `∀p∊ℙ` = 1;
SELECT * from `torture`.`torture` USE INDEX (`BroKen`) WHERE `BroKen` = 2;
SELECT * from `torture`.`torture` USE INDEX (`BroKén`) WHERE `BroKén` = 3;
SELECT * from `torture`.`torture` USE INDEX (`bRŎKȩn`) WHERE `bRŎKȩn` = 4;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME, EVENT_NAME
  FROM performance_schema.events_waits_history_long
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME, EVENT_NAME,
    CONVERT(INDEX_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, EVENT_NAME, CURRENT_SCHEMA
  FROM performance_schema.events_statements_history_long
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, EVENT_NAME;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
  LOCK_TYPE, LOCK_DURATION, LOCK_STATUS
  FROM performance_schema.metadata_locks
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_lock_waits_summary_by_table
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_io_waits_summary_by_table
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME
  FROM performance_schema.table_io_waits_summary_by_index_usage
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME,
    CONVERT(INDEX_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_handles
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.objects_summary_global_by_type
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME;

-- echo "---------------------------------------------------------"
COMMIT;

DROP TABLE `torture`.`torture`;

#
# PROCEDURE NAME torture
#

TRUNCATE TABLE performance_schema.events_waits_history_long;
TRUNCATE TABLE performance_schema.events_statements_history_long;

DELIMITER $$;

CREATE PROCEDURE `torture`.`∀p∊ℙ`()
BEGIN
  SELECT 1;
END
$$

CREATE PROCEDURE `torture`.`BroKen`()
BEGIN
  SELECT 2;
END
$$

-- error ER_SP_ALREADY_EXISTS
CREATE PROCEDURE `torture`.`BroKén`()
BEGIN
  SELECT 3;
END
$$

-- error ER_SP_ALREADY_EXISTS
CREATE PROCEDURE `torture`.`bRŎKȩn`()
BEGIN
  SELECT 4;
END
$$

DELIMITER ;$$

BEGIN;

CALL `torture`.`∀p∊ℙ`;
CALL `torture`.`BroKen`;
CALL `torture`.`BroKén`;
CALL `torture`.`bRŎKȩn`;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME, EVENT_NAME
  FROM performance_schema.events_waits_history_long
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME, EVENT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, EVENT_NAME, CURRENT_SCHEMA
  FROM performance_schema.events_statements_history_long
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, EVENT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
  LOCK_TYPE, LOCK_DURATION, LOCK_STATUS
  FROM performance_schema.metadata_locks
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_lock_waits_summary_by_table
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_io_waits_summary_by_table
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME
  FROM performance_schema.table_io_waits_summary_by_index_usage
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_handles
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.objects_summary_global_by_type
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
COMMIT;

DROP PROCEDURE `torture`.`∀p∊ℙ`;
DROP PROCEDURE `torture`.`BroKen`;

-- error ER_SP_DOES_NOT_EXIST
DROP PROCEDURE `torture`.`BroKén`;

-- error ER_SP_DOES_NOT_EXIST
DROP PROCEDURE `torture`.`bRŎKȩn`;

#
# FUNCTION NAME torture
#

TRUNCATE TABLE performance_schema.events_waits_history_long;
TRUNCATE TABLE performance_schema.events_statements_history_long;

DELIMITER $$;

CREATE FUNCTION `torture`.`∀p∊ℙ`()
RETURNS INTEGER
BEGIN
  RETURN 1;
END
$$

CREATE FUNCTION `torture`.`BroKen`()
RETURNS INTEGER
BEGIN
  RETURN 2;
END
$$

-- error ER_SP_ALREADY_EXISTS
CREATE FUNCTION `torture`.`BroKén`()
RETURNS INTEGER
BEGIN
  RETURN 3;
END
$$

-- error ER_SP_ALREADY_EXISTS
CREATE FUNCTION `torture`.`bRŎKȩn`()
RETURNS INTEGER
BEGIN
  RETURN 4;
END
$$

DELIMITER ;$$

BEGIN;

SELECT `torture`.`∀p∊ℙ`();
SELECT `torture`.`BroKen`();
SELECT `torture`.`BroKén`();
SELECT `torture`.`bRŎKȩn`();

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME, EVENT_NAME
  FROM performance_schema.events_waits_history_long
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME, EVENT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, EVENT_NAME, CURRENT_SCHEMA
  FROM performance_schema.events_statements_history_long
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, EVENT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
  LOCK_TYPE, LOCK_DURATION, LOCK_STATUS
  FROM performance_schema.metadata_locks
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_lock_waits_summary_by_table
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_io_waits_summary_by_table
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME
  FROM performance_schema.table_io_waits_summary_by_index_usage
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.table_handles
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.objects_summary_global_by_type
  WHERE OBJECT_SCHEMA = 'torture'
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME,
    CONVERT(OBJECT_NAME USING 'utf8mb4') COLLATE utf8mb4_bin;

-- echo "---------------------------------------------------------"
COMMIT;

DROP FUNCTION `torture`.`∀p∊ℙ`;
DROP FUNCTION `torture`.`BroKen`;

-- error ER_SP_DOES_NOT_EXIST
DROP FUNCTION `torture`.`BroKén`;

-- error ER_SP_DOES_NOT_EXIST
DROP FUNCTION `torture`.`bRŎKȩn`;

#
# EVENT NAME torture
#

#
# TRIGGER NAME torture
#


CREATE TABLE test.t1(a int);

CREATE TRIGGER test.t1_bi_broken BEFORE INSERT ON test.t1
FOR EACH ROW BEGIN END;

# Force the server to execute the trigger,
# so it gets instrumented.
INSERT INTO test.t1 VALUES (1);

SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.events_statements_summary_by_program
  WHERE OBJECT_SCHEMA='test';

DROP TRIGGER test.`t1_bi_brokén`;

SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
  FROM performance_schema.events_statements_summary_by_program
  WHERE OBJECT_SCHEMA='test';

DROP TABLE test.t1;

DROP SCHEMA `torture`;

UPDATE performance_schema.setup_instruments
  SET ENABLED='YES' WHERE NAME LIKE "wait/synch/%";

