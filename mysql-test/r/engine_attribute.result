# main.engine_attribute BEGIN
#
# WL#13341: Store options for secondary engines.
# Testing engine and secondary engine attributes on tables, fields and 
# indices.
#
### Testing ENGINE_ATTRIBUTE on table
CREATE TABLE t1(i INT) ENGINE_ATTRIBUTE;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '' at line 1
CREATE TABLE t1(i INT) engine_ATTRIBUTE='{bad:json';
ERROR HY000: Invalid json attribute, error: "Missing a name for object member." at pos 1: 'bad:json'
# Testing attributes on tables
CREATE TABLE t1(i INT) ENGINE_ATTRIBUTE='';
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
CREATE TABLE t1(i INT) ENGINE_ATTRIBUTE='{"table attr": "for engine"}';
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
### Testing SECONDARY_ENGINE_ATTRIBUTE on table
CREATE TABLE t1(i INT) SECONDARY_ENGINE_ATTRIBUTE;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '' at line 1
CREATE TABLE t1(i INT) SECONDARY_engine_ATTRIBUTE='{bad:json';
ERROR HY000: Invalid json attribute, error: "Missing a name for object member." at pos 1: 'bad:json'
CREATE TABLE t1(i INT) SECONDARY_ENGINE_ATTRIBUTE='';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
CREATE TABLE t1(i INT) SECONDARY_ENGINE_ATTRIBUTE='{"table attr": "for secondary engine"}';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"table attr": "for secondary engine"}' */
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	NULL	{"table attr": "for secondary engine"}
DROP TABLE t1;
### Testing ENGINE_ATTRIBUTE on column
CREATE TABLE t1(i INT ENGINE_ATTRIBUTE);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')' at line 1
CREATE TABLE t1(i INT ENGINE_ATTRIBUTE='');
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
CREATE TABLE t1(i INT ENGINE_ATTRIBUTE='{"column attr": "for engine"}' SECONDARY_ENGINE_ATTRIBUTE='{"column attr": "for secondary engine"}');
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
### Testing ENGINE_ATTRIBUTE on index specified as a part of CREATE
CREATE TABLE t1(i INT, j INT, INDEX(j) ENGINE_ATTRIBUTE);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')' at line 1
CREATE TABLE t1(i INT, j INT, INDEX(j) ENGINE_ATTRIBUTE='{"index attr": }');
ERROR HY000: Invalid json attribute, error: "Invalid value." at pos 15: '}'
CREATE TABLE t1(i INT, j INT, INDEX(j) ENGINE_ATTRIBUTE='{"index attr": "for engine"}' SECONDARY_ENGINE_ATTRIBUTE='{"index attr": "for secondary engine"}');
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
### Testing ENGINE_ATTRIBUTE with CREATE INDEX
CREATE TABLE t1(i INT, j INT);
CREATE INDEX ix ON t1(i) ENGINE_ATTRIBUTE;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '' at line 1
CREATE INDEX ix ON t1(i) ENGINE_ATTRIBUTE='{attr';
ERROR HY000: Invalid json attribute, error: "Missing a name for object member." at pos 1: 'attr'
CREATE INDEX ix ON t1(i) ENGINE_ATTRIBUTE='{"index attr": "for engine"}' SECONDARY_ENGINE_ATTRIBUTE='{"index attr":"for secondary engine"}';
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW INDEX FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1';
CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	PRIMARY	t1	NULL	NULL
DROP TABLE t1;
### Creating table with all possible engine attributes.
CREATE TABLE t1(i INT
ENGINE_ATTRIBUTE = '{"column attr": "i"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "i"}', 
j INT ENGINE_ATTRIBUTE = '{"column attr": "j"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "j"}', 
INDEX(j) ENGINE_ATTRIBUTE='{"index attr": "index on j"}' 
SECONDARY_ENGINE_ATTRIBUTE='{"secondary index attr": "index on j"}')
ENGINE_ATTRIBUTE = '{"table attr":"t1"}' SECONDARY_ENGINE_ATTRIBUTE = '{"secondary table attr": "t1"}';
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
## Verify engine attributes are duplicated in the same way as comment
## in CREATE LIKE/CREATE AS SELECT
CREATE TABLE t1(i INT
ENGINE_ATTRIBUTE = '{"column attr": "i"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "i"}'
       COMMENT 'Column comment', 
j INT ENGINE_ATTRIBUTE = '{"column attr": "j"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "j"}', 
INDEX(j) ENGINE_ATTRIBUTE='{"index attr": "index on j"}' 
SECONDARY_ENGINE_ATTRIBUTE='{"secondary index attr": "index on j"}' COMMENT 'index comment')
ENGINE_ATTRIBUTE = '{"table attr":"t1"}' SECONDARY_ENGINE_ATTRIBUTE = '{"secondary table attr": "t1"}' COMMENT='Table comment';
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
## Verfy expected behavior for repeated engine attributes
CREATE TABLE t1(i INT
ENGINE_ATTRIBUTE = '{"column attr": "i"}'
       ENGINE_ATTRIBUTE = '{"column attr": "override"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "i"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "override"}'
       COMMENT 'Column comment'
       COMMENT 'Column comment override', 
j INT ENGINE_ATTRIBUTE = '{"column attr": "j"}'
       ENGINE_ATTRIBUTE = '{"column attr": "override"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "j"}' 
SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "override"}', 
INDEX(j) ENGINE_ATTRIBUTE='{"index attr": "index on j"}' ENGINE_ATTRIBUTE='{"index attr": "index on j override"}' 
SECONDARY_ENGINE_ATTRIBUTE='{"secondary index attr": "index on j"}' 
SECONDARY_ENGINE_ATTRIBUTE='{"secondary index attr": "index on j override"}' 
COMMENT 'index comment'
COMMENT 'index comment override'
)
ENGINE_ATTRIBUTE = '{"table attr":"t1"}'
       ENGINE_ATTRIBUTE = '{"table attr":"t1 override"}'	
SECONDARY_ENGINE_ATTRIBUTE = '{"secondary table attr": "t1"}'
       SECONDARY_ENGINE_ATTRIBUTE = '{"secondary table attr": "t1 override"}'
       COMMENT='Table comment'
       COMMENT='Table comment override';
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
#
# Testing altering engine attribute on tables,
# columns and indices.
#
CREATE TABLE t1(i INT, j INT, k INT);
## Altering attributes on table
ALTER TABLE t1 ENGINE_ATTRIBUTE='{"table algo": "none"}';
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	NULL	NULL
ALTER TABLE t1 ENGINE_ATTRIBUTE='{"table algo": "inplace"}',
ALGORITHM=INPLACE;
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	NULL	NULL
ALTER TABLE t1 ENGINE_ATTRIBUTE='{"table algo": "instant"}', 
ALGORITHM=INSTANT;
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
## Adding columns with attributes
ALTER TABLE t1 ADD COLUMN l VARCHAR(64) ENGINE_ATTRIBUTE='{"add column algo":"none"}';
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
ALTER TABLE t1 ADD COLUMN m VARCHAR(64) ENGINE_ATTRIBUTE='{"add column algo":"inplace"}', ALGORITHM=INPLACE;
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
ALTER TABLE t1 ADD COLUMN n VARCHAR(64) ENGINE_ATTRIBUTE='{"add column algo":"instant"}', ALGORITHM=INSTANT;
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
## Modifying columns with attributes
ALTER TABLE t1 MODIFY COLUMN l INT ENGINE_ATTRIBUTE='{"modify column algo": "none"}';
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
ALTER TABLE t1 MODIFY COLUMN m INT ENGINE_ATTRIBUTE='{"modify column algo": "inplace"}', ALGORITHM=INPLACE;
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
ALTER TABLE t1 MODIFY COLUMN n INT ENGINE_ATTRIBUTE='{"modify column algo": "instant"}', ALGORITHM=INSTANT;
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
## Changing columns with attributes
ALTER TABLE t1 CHANGE COLUMN l ll CHAR(8) ENGINE_ATTRIBUTE='{"change column algo": "none"}';
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
ALTER TABLE t1 CHANGE COLUMN m mm CHAR(8) ENGINE_ATTRIBUTE='{"change column algo": "inplace" }', ALGORITHM=INPLACE;
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
ALTER TABLE t1 CHANGE COLUMN n nn CHAR(8) ENGINE_ATTRIBUTE='{"change column algo": "instant" }', ALGORITHM=INSTANT;
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
## Adding index with attributes
ALTER TABLE t1 ADD INDEX (i) ENGINE_ATTRIBUTE='{"index algo":"none"}';
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1';
CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	PRIMARY	t1	NULL	NULL
ALTER TABLE t1 ADD INDEX (m) ENGINE_ATTRIBUTE='{"index algo":"inplace"}', ALGORITHM=INPLACE;
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
ALTER TABLE t1 ADD INDEX (n) ENGINE_ATTRIBUTE='{"index algo":"instant"}', ALGORITHM=INSTANT;
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
DROP TABLE t1;
#
# Testing altering secondary engine attribute on tables and
# columns, and indices.
#
CREATE TABLE t1(i INT, j INT, k INT);
## Altering attributes on table
ALTER TABLE t1 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "none"}';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "none"}' */
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	NULL	{"table algo": "none"}
ALTER TABLE t1 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}',
ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	NULL	{"table algo": "inplace"}
ALTER TABLE t1 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "instant"}', 
ALGORITHM=INSTANT;
ERROR 0A000: ALGORITHM=INSTANT is not supported for this operation. Try ALGORITHM=COPY/INPLACE.
## Adding columns with attributes
ALTER TABLE t1 ADD COLUMN l VARCHAR(64) SECONDARY_ENGINE_ATTRIBUTE='{"add column algo":"none"}';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `l` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "none"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	l	NULL	{"add column algo": "none"}
ALTER TABLE t1 ADD COLUMN m VARCHAR(64) SECONDARY_ENGINE_ATTRIBUTE='{"add column algo":"inplace"}', ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `l` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	l	NULL	{"add column algo": "none"}
def	test	t1	m	NULL	{"add column algo": "inplace"}
ALTER TABLE t1 ADD COLUMN n VARCHAR(64) SECONDARY_ENGINE_ATTRIBUTE='{"add column algo":"instant"}', ALGORITHM=INSTANT;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `l` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	l	NULL	{"add column algo": "none"}
def	test	t1	m	NULL	{"add column algo": "inplace"}
def	test	t1	n	NULL	{"add column algo": "instant"}
## Modifying columns with attributes
ALTER TABLE t1 MODIFY COLUMN l INT SECONDARY_ENGINE_ATTRIBUTE='{"modify column algo": "none"}';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `l` int DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"modify column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	l	NULL	{"modify column algo": "none"}
def	test	t1	m	NULL	{"add column algo": "inplace"}
def	test	t1	n	NULL	{"add column algo": "instant"}
ALTER TABLE t1 MODIFY COLUMN m INT SECONDARY_ENGINE_ATTRIBUTE='{"modify column algo": "inplace"}', ALGORITHM=INPLACE;
ERROR 0A000: ALGORITHM=INPLACE is not supported. Reason: Cannot change column type INPLACE. Try ALGORITHM=COPY.
ALTER TABLE t1 MODIFY COLUMN m INT SECONDARY_ENGINE_ATTRIBUTE='{"modify column algo": "inplace"}', ALGORITHM=INPLACE;
ERROR 0A000: ALGORITHM=INPLACE is not supported. Reason: Cannot change column type INPLACE. Try ALGORITHM=COPY.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `l` int DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"modify column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	l	NULL	{"modify column algo": "none"}
def	test	t1	m	NULL	{"add column algo": "inplace"}
def	test	t1	n	NULL	{"add column algo": "instant"}
ALTER TABLE t1 MODIFY COLUMN n INT SECONDARY_ENGINE_ATTRIBUTE='{"modify column algo": "instant"}', ALGORITHM=INSTANT;
ERROR 0A000: ALGORITHM=INSTANT is not supported. Reason: Need to rebuild the table to change column type. Try ALGORITHM=COPY/INPLACE.
ALTER TABLE t1 MODIFY COLUMN n INT SECONDARY_ENGINE_ATTRIBUTE='{"modify column algo": "instant"}', ALGORITHM=INSTANT;
ERROR 0A000: ALGORITHM=INSTANT is not supported. Reason: Need to rebuild the table to change column type. Try ALGORITHM=COPY/INPLACE.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `l` int DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"modify column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	l	NULL	{"modify column algo": "none"}
def	test	t1	m	NULL	{"add column algo": "inplace"}
def	test	t1	n	NULL	{"add column algo": "instant"}
## Changing columns with attributes
ALTER TABLE t1 CHANGE COLUMN l ll CHAR(8) SECONDARY_ENGINE_ATTRIBUTE='{"change column algo": "none"}';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `ll` char(8) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"change column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	ll	NULL	{"change column algo": "none"}
def	test	t1	m	NULL	{"add column algo": "inplace"}
def	test	t1	n	NULL	{"add column algo": "instant"}
ALTER TABLE t1 CHANGE COLUMN m mm CHAR(8) SECONDARY_ENGINE_ATTRIBUTE='{"change column algo": "inplace" }', ALGORITHM=INPLACE;
ERROR 0A000: ALGORITHM=INPLACE is not supported. Reason: Cannot change column type INPLACE. Try ALGORITHM=COPY.
ALTER TABLE t1 CHANGE COLUMN m mm CHAR(8) SECONDARY_ENGINE_ATTRIBUTE='{"change column algo": "inplace" }', ALGORITHM=INPLACE;
ERROR 0A000: ALGORITHM=INPLACE is not supported. Reason: Cannot change column type INPLACE. Try ALGORITHM=COPY.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `ll` char(8) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"change column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	ll	NULL	{"change column algo": "none"}
def	test	t1	m	NULL	{"add column algo": "inplace"}
def	test	t1	n	NULL	{"add column algo": "instant"}
ALTER TABLE t1 CHANGE COLUMN n nn CHAR(8) SECONDARY_ENGINE_ATTRIBUTE='{"change column algo": "instant" }', ALGORITHM=INSTANT;
ERROR 0A000: ALGORITHM=INSTANT is not supported. Reason: Need to rebuild the table to change column type. Try ALGORITHM=COPY/INPLACE.
ALTER TABLE t1 CHANGE COLUMN n nn CHAR(8) SECONDARY_ENGINE_ATTRIBUTE='{"change column algo": "instant" }', ALGORITHM=INSTANT;
ERROR 0A000: ALGORITHM=INSTANT is not supported. Reason: Need to rebuild the table to change column type. Try ALGORITHM=COPY/INPLACE.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `ll` char(8) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"change column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	ll	NULL	{"change column algo": "none"}
def	test	t1	m	NULL	{"add column algo": "inplace"}
def	test	t1	n	NULL	{"add column algo": "instant"}
## Adding index with attributes
ALTER TABLE t1 ADD INDEX (i) SECONDARY_ENGINE_ATTRIBUTE='{"index algo":"none"}';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `ll` char(8) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"change column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */,
  KEY `i` (`i`) /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"index algo": "none"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1';
CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	PRIMARY	t1	NULL	NULL
def	test	i	t1	NULL	{"index algo": "none"}
ALTER TABLE t1 ADD INDEX (m) SECONDARY_ENGINE_ATTRIBUTE='{"index algo":"inplace"}', ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `ll` char(8) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"change column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */,
  KEY `i` (`i`) /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"index algo": "none"}' */,
  KEY `m` (`m`) /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"index algo": "inplace"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1';
CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	PRIMARY	t1	NULL	NULL
def	test	i	t1	NULL	{"index algo": "none"}
def	test	m	t1	NULL	{"index algo": "inplace"}
ALTER TABLE t1 ADD INDEX (n) SECONDARY_ENGINE_ATTRIBUTE='{"index algo":"instant"}', ALGORITHM=INSTANT;
ERROR 0A000: ALGORITHM=INSTANT is not supported for this operation. Try ALGORITHM=COPY/INPLACE.
DROP TABLE t1;
# Testing all possible engine attribute clauses together
#
CREATE TABLE t1(i INT, j INT, INDEX jix (j));
ALTER TABLE t1 MODIFY COLUMN i INT
ENGINE_ATTRIBUTE = '{"column attr": "i"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "i"}',
MODIFY COLUMN j INT ENGINE_ATTRIBUTE = '{"column attr": "j"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "j"}',
DROP INDEX jix,
ADD INDEX jix (i) ENGINE_ATTRIBUTE='{"index attr": "index on j"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary index attr": "index on j"}',
ENGINE_ATTRIBUTE = '{"table attr":"t1"}'
       SECONDARY_ENGINE_ATTRIBUTE = '{"secondary table attr": "t1"}';
ERROR HY000: Storage engine 'InnoDB' does not support ENGINE_ATTRIBUTE.
DROP TABLE t1;
# Verfy that handler::check_if_supported_inplace_alter
# (default if not overridden by SE) behaves the same way for
# engine attributes as for comments.
CREATE TABLE t1(i INT, j INT, INDEX jix (j)) ENGINE=MYISAM;
ALTER TABLE t1 COMMENT 'A comment', ALGORITHM=INPLACE;
ALTER TABLE t1
SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "i"}',
MODIFY COLUMN j INT ENGINE_ATTRIBUTE = '{"column attr": "j"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary column attr": "j"}',
DROP INDEX jix,
ADD INDEX jix (i) ENGINE_ATTRIBUTE='{"index attr": "index on j"}'
       SECONDARY_ENGINE_ATTRIBUTE='{"secondary index attr": "index on j"}',
ENGINE_ATTRIBUTE = '{"table attr":"t1"}'
       SECONDARY_ENGINE_ATTRIBUTE = '{"secondary table attr": "t1"}';
ERROR HY000: Storage engine 'MyISAM' does not support ENGINE_ATTRIBUTE.
DROP TABLE t1;
# Verfy that handler::check_if_supported_inplace_alter
# (default if not overridden by SE) behaves the same way for
# engine attributes as for comments.
CREATE TABLE t1(i INT, j INT) ENGINE=MYISAM;
ALTER TABLE t1 COMMENT 'A comment', ALGORITHM=INSTANT;
ALTER TABLE t1 ENGINE_ATTRIBUTE='{"table algo": "inplace"}',
ALGORITHM=INSTANT;
ERROR HY000: Storage engine 'MyISAM' does not support ENGINE_ATTRIBUTE.
ALTER TABLE t1 MODIFY COLUMN i INT COMMENT 'Modified column with comment', ALGORITHM=INSTANT;
ALTER TABLE t1 MODIFY COLUMN i INT ENGINE_ATTRIBUTE='{"modify column algo": "inplace"}', ALGORITHM=INSTANT;
ERROR HY000: Storage engine 'MyISAM' does not support ENGINE_ATTRIBUTE.
ALTER TABLE t1 CHANGE COLUMN i mm INT COMMENT 'Changed column with comment', ALGORITHM=INSTANT;
ALTER TABLE t1 CHANGE COLUMN j nn INT ENGINE_ATTRIBUTE='{"change column algo": "inplace" }', ALGORITHM=INSTANT;
ERROR HY000: Storage engine 'MyISAM' does not support ENGINE_ATTRIBUTE.
DROP TABLE t1;
# Verfy that handler::check_if_supported_inplace_alter
# (default if not overridden by SE) behaves the same way for
# secondary engine attributes as for comments.
CREATE TABLE t1(i INT, j INT) ENGINE=MYISAM;
ALTER TABLE t1 COMMENT 'A comment', ALGORITHM=INSTANT;
ALTER TABLE t1 SECONDARY_ENGINE_ATTRIBUTE='{"table algo": "inplace"}',
ALGORITHM=INSTANT;
ALTER TABLE t1 MODIFY COLUMN i INT COMMENT 'Modified column with comment', ALGORITHM=INSTANT;
ALTER TABLE t1 MODIFY COLUMN i INT SECONDARY_ENGINE_ATTRIBUTE='{"modify column algo": "inplace"}', ALGORITHM=INSTANT;
ALTER TABLE t1 CHANGE COLUMN i mm INT COMMENT 'Changed column with comment', ALGORITHM=INSTANT;
ALTER TABLE t1 CHANGE COLUMN j nn INT SECONDARY_ENGINE_ATTRIBUTE='{"change column algo": "inplace" }', ALGORITHM=INSTANT;
DROP TABLE t1;
# main.engine_attribute END
