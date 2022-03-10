#
# Bug#33148961 FAILURE TO UPGRADE FROM 5.7, INVALID UTF8 CHARACTER STRING
#
# Test for invalid comment strings when creating table, field, index,
# partition, subpartition, tablespace, procedure, function, event.
#
SET NAMES utf8mb3;
Warnings:
Warning	1287	'utf8mb3' is deprecated and will be removed in a future release. Please use utf8mb4 instead

# Test CREATE statements with invalid comments.

CREATE TABLE t1 (a int) COMMENT 'tab🐬';
ERROR HY000: Comment for table 'test.t1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

CREATE TABLE t2 (a int COMMENT 'col🐬');
ERROR HY000: Comment for field 'test.t2.a' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

CREATE TABLE t3 (a int, INDEX idx1(a) COMMENT 'idx🐬');
ERROR HY000: Comment for index 'test.t3.idx1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

CREATE TABLE t4 (a int) PARTITION BY RANGE (a) (PARTITION p1 VALUES LESS THAN (0) COMMENT 'part🐬');
ERROR HY000: Comment for partition 'test.t4.p1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

CREATE TABLE t5 (a int) PARTITION BY RANGE (a) SUBPARTITION BY HASH(a) SUBPARTITIONS 1 (PARTITION p1 VALUES LESS THAN (0)(SUBPARTITION sp1 COMMENT 'subpart🐬'));
ERROR HY000: Comment for subpartition 'test.t5.p1.sp1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

CREATE VIEW v1 AS SELECT 'view🐬';
ERROR HY000: Definition of view 'test.v1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

CREATE PROCEDURE sp1() COMMENT 'proc🐬' BEGIN END;
ERROR HY000: Comment for stored routine 'test.sp1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

CREATE FUNCTION sf1() RETURNS INT DETERMINISTIC COMMENT 'func🐬' RETURN 0;
ERROR HY000: Comment for stored routine 'test.sf1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

CREATE EVENT evt1 ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR COMMENT 'evt🐬' DO SELECT 0;
ERROR HY000: Comment for event 'test.evt1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

# Test ALTER statements with invalid comments.

CREATE TABLE t1 (a int);

ALTER TABLE t1 COMMENT 'tab🐬';
ERROR HY000: Comment for table 'test.t1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

ALTER TABLE t1 MODIFY a int COMMENT 'col🐬';
ERROR HY000: Comment for field 'test.t1.a' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

ALTER TABLE t1 ADD b int COMMENT 'col🐬';
ERROR HY000: Comment for field 'test.t1.b' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

ALTER TABLE t1 ADD INDEX idx1(a) COMMENT 'idx🐬';
ERROR HY000: Comment for index 'test.t1.idx1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

ALTER TABLE t1 PARTITION BY RANGE (a) (PARTITION p1 VALUES LESS THAN (0) COMMENT 'part🐬');
ERROR HY000: Comment for partition 'test.t1.p1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

ALTER TABLE t1 PARTITION BY RANGE (a) SUBPARTITION BY HASH(a) SUBPARTITIONS 1 (PARTITION p1 VALUES LESS THAN (0)(SUBPARTITION sp1 COMMENT 'subpart🐬'));
ERROR HY000: Comment for subpartition 'test.t1.p1.sp1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

DROP TABLE t1;

CREATE VIEW v1 AS SELECT 0;
ALTER VIEW v1 AS SELECT 'view🐬';
ERROR HY000: Definition of view 'test.v1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.
DROP VIEW v1;

CREATE PROCEDURE sp1() BEGIN END;
ALTER PROCEDURE sp1 COMMENT 'proc🐬';
ERROR HY000: Comment for stored routine 'test.sp1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.
DROP PROCEDURE sp1;

CREATE FUNCTION sf1() RETURNS INT DETERMINISTIC RETURN 0;
ALTER FUNCTION sf1 COMMENT 'func🐬';
ERROR HY000: Comment for stored routine 'test.sf1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.
DROP FUNCTION sf1;

CREATE EVENT evt1 ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR DO SELECT 0;
ALTER EVENT evt1 COMMENT 'evt🐬';
ERROR HY000: Comment for event 'test.evt1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.
DROP EVENT evt1;

# Test invalid comment on TABLESPACE

CREATE TABLESPACE ts1 ADD DATAFILE 'df1.ibd' COMMENT 'ts🐬';
ERROR HY000: Comment for tablespace 'ts1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

# Test invalid characters in stored routine body definition
CREATE PROCEDURE p1() BEGIN /* 'SP body comment: 🐬' */ END;
ERROR HY000: Definition of stored routine 'test.p1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.
CREATE FUNCTION f1() RETURNS INT DETERMINISTIC RETURN /* 'SF body comment: 🐬' */ 0;
ERROR HY000: Definition of stored routine 'test.f1' contains an invalid utf8mb3 character string: '\xF0\x9F\x90'.

# Cleanup
SET NAMES DEFAULT;
