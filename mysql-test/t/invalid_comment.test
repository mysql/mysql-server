## This file contains BINARY DATA.
## EDIT WITH CARE USING AN APPROPRIATE TOOL.

--echo #
--echo # Bug#33148961 FAILURE TO UPGRADE FROM 5.7, INVALID utf8mb3 CHARACTER STRING
--echo #
--echo # Test for invalid comment strings when creating table, field, index,
--echo # partition, subpartition, tablespace, procedure, function, event.
--echo #

# Set the client charset equal to the system charset. This is done to avoid the
# conversion of string literals by the parser when the charset differs.
SET NAMES utf8mb3;

# To allow testing of invalid binary data in the comment strings
--character_set binary

--echo
--echo # Test CREATE statements with invalid comments.

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
CREATE TABLE t1 (a int) COMMENT 'tab🐬';

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
CREATE TABLE t2 (a int COMMENT 'col🐬');

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
CREATE TABLE t3 (a int, INDEX idx1(a) COMMENT 'idx🐬');

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
CREATE TABLE t4 (a int) PARTITION BY RANGE (a) (PARTITION p1 VALUES LESS THAN (0) COMMENT 'part🐬');

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
CREATE TABLE t5 (a int) PARTITION BY RANGE (a) SUBPARTITION BY HASH(a) SUBPARTITIONS 1 (PARTITION p1 VALUES LESS THAN (0)(SUBPARTITION sp1 COMMENT 'subpart🐬'));

--echo
--error ER_DEFINITION_CONTAINS_INVALID_STRING
CREATE VIEW v1 AS SELECT 'view🐬';

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
CREATE PROCEDURE sp1() COMMENT 'proc🐬' BEGIN END;

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
CREATE FUNCTION sf1() RETURNS INT DETERMINISTIC COMMENT 'func🐬' RETURN 0;

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
CREATE EVENT evt1 ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR COMMENT 'evt🐬' DO SELECT 0;

--echo
--echo # Test ALTER statements with invalid comments.

--echo
CREATE TABLE t1 (a int);

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
ALTER TABLE t1 COMMENT 'tab🐬';

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
ALTER TABLE t1 MODIFY a int COMMENT 'col🐬';

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
ALTER TABLE t1 ADD b int COMMENT 'col🐬';

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
ALTER TABLE t1 ADD INDEX idx1(a) COMMENT 'idx🐬';

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
ALTER TABLE t1 PARTITION BY RANGE (a) (PARTITION p1 VALUES LESS THAN (0) COMMENT 'part🐬');

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
ALTER TABLE t1 PARTITION BY RANGE (a) SUBPARTITION BY HASH(a) SUBPARTITIONS 1 (PARTITION p1 VALUES LESS THAN (0)(SUBPARTITION sp1 COMMENT 'subpart🐬'));

--echo
DROP TABLE t1;

--echo
CREATE VIEW v1 AS SELECT 0;
--error ER_DEFINITION_CONTAINS_INVALID_STRING
ALTER VIEW v1 AS SELECT 'view🐬';
DROP VIEW v1;

--echo
CREATE PROCEDURE sp1() BEGIN END;
--error ER_COMMENT_CONTAINS_INVALID_STRING
ALTER PROCEDURE sp1 COMMENT 'proc🐬';
DROP PROCEDURE sp1;

--echo
CREATE FUNCTION sf1() RETURNS INT DETERMINISTIC RETURN 0;
--error ER_COMMENT_CONTAINS_INVALID_STRING
ALTER FUNCTION sf1 COMMENT 'func🐬';
DROP FUNCTION sf1;

--echo
CREATE EVENT evt1 ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR DO SELECT 0;
--error ER_COMMENT_CONTAINS_INVALID_STRING
ALTER EVENT evt1 COMMENT 'evt🐬';
DROP EVENT evt1;

--echo
--echo # Test invalid comment on TABLESPACE

--echo
--error ER_COMMENT_CONTAINS_INVALID_STRING
CREATE TABLESPACE ts1 ADD DATAFILE 'df1.ibd' COMMENT 'ts🐬';

--echo
--echo # Test invalid characters in stored routine body definition
--error ER_DEFINITION_CONTAINS_INVALID_STRING
CREATE PROCEDURE p1() BEGIN /* 'SP body comment: 🐬' */ END;
--error ER_DEFINITION_CONTAINS_INVALID_STRING
CREATE FUNCTION f1() RETURNS INT DETERMINISTIC RETURN /* 'SF body comment: 🐬' */ 0;

--echo
--echo # Cleanup
--character_set utf8mb4
SET NAMES DEFAULT;
