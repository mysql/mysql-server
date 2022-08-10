# main.engine_attribute BEGIN
SET SESSION debug= "+d,simulate_engine_attribute_support";
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
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	NULL	NULL
DROP TABLE t1;
CREATE TABLE t1(i INT) ENGINE_ATTRIBUTE='{"table attr": "for engine"}';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table attr": "for engine"}' */
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	{"table attr": "for engine"}	NULL
DROP TABLE t1;
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
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW FULL COLUMNS FROM t1;
Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment
i	int	NULL	YES		NULL		select,insert,update,references	
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
DROP TABLE t1;
CREATE TABLE t1(i INT ENGINE_ATTRIBUTE='{"column attr": "for engine"}' SECONDARY_ENGINE_ATTRIBUTE='{"column attr": "for secondary engine"}');
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"column attr": "for engine"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"column attr": "for secondary engine"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW FULL COLUMNS FROM t1;
Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment
i	int	NULL	YES		NULL		select,insert,update,references	
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	{"column attr": "for engine"}	{"column attr": "for secondary engine"}
DROP TABLE t1;
### Testing ENGINE_ATTRIBUTE on index specified as a part of CREATE
CREATE TABLE t1(i INT, j INT, INDEX(j) ENGINE_ATTRIBUTE);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ')' at line 1
CREATE TABLE t1(i INT, j INT, INDEX(j) ENGINE_ATTRIBUTE='{"index attr": }');
ERROR HY000: Invalid json attribute, error: "Invalid value." at pos 15: '}'
CREATE TABLE t1(i INT, j INT, INDEX(j) ENGINE_ATTRIBUTE='{"index attr": "for engine"}' SECONDARY_ENGINE_ATTRIBUTE='{"index attr": "for secondary engine"}');
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  KEY `j` (`j`) /*!80021 ENGINE_ATTRIBUTE '{"index attr": "for engine"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"index attr": "for secondary engine"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW INDEX FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	j	1	j	A	0	NULL	NULL	YES	BTREE			YES	NULL
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1';
CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	PRIMARY	t1	NULL	NULL
def	test	j	t1	{"index attr": "for engine"}	{"index attr": "for secondary engine"}
DROP TABLE t1;
### Testing ENGINE_ATTRIBUTE with CREATE INDEX
CREATE TABLE t1(i INT, j INT);
CREATE INDEX ix ON t1(i) ENGINE_ATTRIBUTE;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '' at line 1
CREATE INDEX ix ON t1(i) ENGINE_ATTRIBUTE='{attr';
ERROR HY000: Invalid json attribute, error: "Missing a name for object member." at pos 1: 'attr'
CREATE INDEX ix ON t1(i) ENGINE_ATTRIBUTE='{"index attr": "for engine"}' SECONDARY_ENGINE_ATTRIBUTE='{"index attr":"for secondary engine"}';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  KEY `ix` (`i`) /*!80021 ENGINE_ATTRIBUTE '{"index attr": "for engine"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"index attr": "for secondary engine"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW INDEX FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	ix	1	i	A	0	NULL	NULL	YES	BTREE			YES	NULL
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1';
CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	PRIMARY	t1	NULL	NULL
def	test	ix	t1	{"index attr": "for engine"}	{"index attr": "for secondary engine"}
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
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"column attr": "i"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "i"}' */,
  `j` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"column attr": "j"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "j"}' */,
  KEY `j` (`j`) /*!80021 ENGINE_ATTRIBUTE '{"index attr": "index on j"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary index attr": "index on j"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table attr": "t1"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"secondary table attr": "t1"}' */
SHOW FULL COLUMNS FROM t1;
Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment
i	int	NULL	YES		NULL		select,insert,update,references	
j	int	NULL	YES	MUL	NULL		select,insert,update,references	
SHOW INDEX FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	j	1	j	A	0	NULL	NULL	YES	BTREE			YES	NULL
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	{"table attr": "t1"}	{"secondary table attr": "t1"}
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1' ORDER BY column_name;
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	{"column attr": "i"}	{"secondary column attr": "i"}
def	test	t1	j	{"column attr": "j"}	{"secondary column attr": "j"}
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1' ORDER BY constraint_name;
CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	PRIMARY	t1	NULL	NULL
def	test	j	t1	{"index attr": "index on j"}	{"secondary index attr": "index on j"}
# Verify that SHOW CREATE TABLE output can be used to create
# an identical table
DROP TABLE t1;
CREATE TABLE `t1` (
  `i` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"column attr": "i"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "i"}' */,
  `j` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"column attr": "j"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "j"}' */,
  KEY `j` (`j`) /*!80021 ENGINE_ATTRIBUTE '{"index attr": "index on j"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary index attr": "index on j"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table attr": "t1"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"secondary table attr": "t1"}' */;
# There should be no output beween here
# ... and here
DROP TABLE t1;
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
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL COMMENT 'Column comment' /*!80021 ENGINE_ATTRIBUTE '{"column attr": "i"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "i"}' */,
  `j` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"column attr": "j"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "j"}' */,
  KEY `j` (`j`) COMMENT 'index comment' /*!80021 ENGINE_ATTRIBUTE '{"index attr": "index on j"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary index attr": "index on j"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Table comment' /*!80021 ENGINE_ATTRIBUTE='{"table attr": "t1"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"secondary table attr": "t1"}' */
SHOW FULL COLUMNS FROM t1;
Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment
i	int	NULL	YES		NULL		select,insert,update,references	Column comment
j	int	NULL	YES	MUL	NULL		select,insert,update,references	
SHOW INDEX FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	j	1	j	A	0	NULL	NULL	YES	BTREE		index comment	YES	NULL
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	{"table attr": "t1"}	{"secondary table attr": "t1"}
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1' ORDER BY column_name;
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	{"column attr": "i"}	{"secondary column attr": "i"}
def	test	t1	j	{"column attr": "j"}	{"secondary column attr": "j"}
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1' ORDER BY constraint_name;
CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	PRIMARY	t1	NULL	NULL
def	test	j	t1	{"index attr": "index on j"}	{"secondary index attr": "index on j"}
CREATE TABLE t2 LIKE t1;
# Expect all engine attributes from t1 to be present also in t2
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `i` int DEFAULT NULL COMMENT 'Column comment' /*!80021 ENGINE_ATTRIBUTE '{"column attr": "i"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "i"}' */,
  `j` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"column attr": "j"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "j"}' */,
  KEY `j` (`j`) COMMENT 'index comment' /*!80021 ENGINE_ATTRIBUTE '{"index attr": "index on j"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary index attr": "index on j"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Table comment' /*!80021 ENGINE_ATTRIBUTE='{"table attr": "t1"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"secondary table attr": "t1"}' */
SHOW FULL COLUMNS FROM t2;
Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment
i	int	NULL	YES		NULL		select,insert,update,references	Column comment
j	int	NULL	YES	MUL	NULL		select,insert,update,references	
SHOW INDEX FROM t2;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t2	1	j	1	j	A	0	NULL	NULL	YES	BTREE		index comment	YES	NULL
CREATE TABLE t3 ENGINE_ATTRIBUTE='{"table attr": "t3, create as select"}' AS SELECT * FROM t1;
# Expect column engine attributes from t1 to be present in t3.
# Indices are not duplicated so index engine attributes will not be
# present.
# Table engine attributes are not duplicated, just like table comments.
SHOW CREATE TABLE t3;
Table	Create Table
t3	CREATE TABLE `t3` (
  `i` int DEFAULT NULL COMMENT 'Column comment' /*!80021 ENGINE_ATTRIBUTE '{"column attr": "i"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "i"}' */,
  `j` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"column attr": "j"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "j"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table attr": "t3, create as select"}' */
SHOW FULL COLUMNS FROM t3;
Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment
i	int	NULL	YES		NULL		select,insert,update,references	Column comment
j	int	NULL	YES		NULL		select,insert,update,references	
SHOW INDEX FROM t3;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't3';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t3	{"table attr": "t3, create as select"}	NULL
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't3' ORDER BY column_name;
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t3	i	{"column attr": "i"}	{"secondary column attr": "i"}
def	test	t3	j	{"column attr": "j"}	{"secondary column attr": "j"}
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't3' ORDER BY constraint_name;
CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	PRIMARY	t3	NULL	NULL
DROP TABLE t3;
DROP TABLE t2;
DROP TABLE t1;
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
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL COMMENT 'Column comment override' /*!80021 ENGINE_ATTRIBUTE '{"column attr": "override"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "override"}' */,
  `j` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"column attr": "override"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "override"}' */,
  KEY `j` (`j`) COMMENT 'index comment override' /*!80021 ENGINE_ATTRIBUTE '{"index attr": "index on j override"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary index attr": "index on j override"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Table comment override' /*!80021 ENGINE_ATTRIBUTE='{"table attr": "t1 override"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"secondary table attr": "t1 override"}' */
SHOW FULL COLUMNS FROM t1;
Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment
i	int	NULL	YES		NULL		select,insert,update,references	Column comment override
j	int	NULL	YES	MUL	NULL		select,insert,update,references	
SHOW INDEX FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	j	1	j	A	0	NULL	NULL	YES	BTREE		index comment override	YES	NULL
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	{"table attr": "t1 override"}	{"secondary table attr": "t1 override"}
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1' ORDER BY column_name;
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	{"column attr": "override"}	{"secondary column attr": "override"}
def	test	t1	j	{"column attr": "override"}	{"secondary column attr": "override"}
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1' ORDER BY constraint_name;
CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	PRIMARY	t1	NULL	NULL
def	test	j	t1	{"index attr": "index on j override"}	{"secondary index attr": "index on j override"}
DROP TABLE t1;
#
# Testing altering engine attribute on tables,
# columns and indices.
#
CREATE TABLE t1(i INT, j INT, k INT);
## Altering attributes on table
ALTER TABLE t1 ENGINE_ATTRIBUTE='{"table algo": "none"}';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table algo": "none"}' */
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	{"table algo": "none"}	NULL
ALTER TABLE t1 ENGINE_ATTRIBUTE='{"table algo": "inplace"}',
ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	{"table algo": "inplace"}	NULL
ALTER TABLE t1 ENGINE_ATTRIBUTE='{"table algo": "instant"}', 
ALGORITHM=INSTANT;
ERROR 0A000: ALGORITHM=INSTANT is not supported for this operation. Try ALGORITHM=COPY/INPLACE.
## Adding columns with attributes
ALTER TABLE t1 ADD COLUMN l VARCHAR(64) ENGINE_ATTRIBUTE='{"add column algo":"none"}';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `l` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "none"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	l	{"add column algo": "none"}	NULL
ALTER TABLE t1 ADD COLUMN m VARCHAR(64) ENGINE_ATTRIBUTE='{"add column algo":"inplace"}', ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `l` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	l	{"add column algo": "none"}	NULL
def	test	t1	m	{"add column algo": "inplace"}	NULL
ALTER TABLE t1 ADD COLUMN n VARCHAR(64) ENGINE_ATTRIBUTE='{"add column algo":"instant"}', ALGORITHM=INSTANT;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `l` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	l	{"add column algo": "none"}	NULL
def	test	t1	m	{"add column algo": "inplace"}	NULL
def	test	t1	n	{"add column algo": "instant"}	NULL
## Modifying columns with attributes
ALTER TABLE t1 MODIFY COLUMN l INT ENGINE_ATTRIBUTE='{"modify column algo": "none"}';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `l` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"modify column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	l	{"modify column algo": "none"}	NULL
def	test	t1	m	{"add column algo": "inplace"}	NULL
def	test	t1	n	{"add column algo": "instant"}	NULL
ALTER TABLE t1 MODIFY COLUMN m INT ENGINE_ATTRIBUTE='{"modify column algo": "inplace"}', ALGORITHM=INPLACE;
ERROR 0A000: ALGORITHM=INPLACE is not supported. Reason: Cannot change column type INPLACE. Try ALGORITHM=COPY.
ALTER TABLE t1 MODIFY COLUMN n INT ENGINE_ATTRIBUTE='{"modify column algo": "instant"}', ALGORITHM=INSTANT;
ERROR 0A000: ALGORITHM=INSTANT is not supported. Reason: Need to rebuild the table to change column type. Try ALGORITHM=COPY/INPLACE.
## Changing columns with attributes
ALTER TABLE t1 CHANGE COLUMN m mm CHAR(8) ENGINE_ATTRIBUTE='{"change column algo": "inplace" }', ALGORITHM=INPLACE;
ERROR 0A000: ALGORITHM=INPLACE is not supported. Reason: Cannot change column type INPLACE. Try ALGORITHM=COPY.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `l` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"modify column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	l	{"modify column algo": "none"}	NULL
def	test	t1	m	{"add column algo": "inplace"}	NULL
def	test	t1	n	{"add column algo": "instant"}	NULL
ALTER TABLE t1 CHANGE COLUMN n nn CHAR(8) ENGINE_ATTRIBUTE='{"change column algo": "instant" }', ALGORITHM=INSTANT;
ERROR 0A000: ALGORITHM=INSTANT is not supported. Reason: Need to rebuild the table to change column type. Try ALGORITHM=COPY/INPLACE.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `l` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"modify column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	NULL	NULL
def	test	t1	k	NULL	NULL
def	test	t1	l	{"modify column algo": "none"}	NULL
def	test	t1	m	{"add column algo": "inplace"}	NULL
def	test	t1	n	{"add column algo": "instant"}	NULL
## Adding index with attributes
ALTER TABLE t1 ADD INDEX (i) ENGINE_ATTRIBUTE='{"index algo":"none"}';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `l` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"modify column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */,
  KEY `i` (`i`) /*!80021 ENGINE_ATTRIBUTE '{"index algo": "none"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1';
CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	PRIMARY	t1	NULL	NULL
def	test	i	t1	{"index algo": "none"}	NULL
ALTER TABLE t1 ADD INDEX (m) ENGINE_ATTRIBUTE='{"index algo":"inplace"}', ALGORITHM=INPLACE;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL,
  `k` int DEFAULT NULL,
  `l` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"modify column algo": "none"}' */,
  `m` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "inplace"}' */,
  `n` varchar(64) DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"add column algo": "instant"}' */,
  KEY `i` (`i`) /*!80021 ENGINE_ATTRIBUTE '{"index algo": "none"}' */,
  KEY `m` (`m`) /*!80021 ENGINE_ATTRIBUTE '{"index algo": "inplace"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table algo": "inplace"}' */
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1';
CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	PRIMARY	t1	NULL	NULL
def	test	i	t1	{"index algo": "none"}	NULL
def	test	m	t1	{"index algo": "inplace"}	NULL
ALTER TABLE t1 ADD INDEX (n) ENGINE_ATTRIBUTE='{"index algo":"instant"}', ALGORITHM=INSTANT;
ERROR 0A000: ALGORITHM=INSTANT is not supported for this operation. Try ALGORITHM=COPY/INPLACE.
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
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"column attr": "i"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "i"}' */,
  `j` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"column attr": "j"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "j"}' */,
  KEY `jix` (`i`) /*!80021 ENGINE_ATTRIBUTE '{"index attr": "index on j"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary index attr": "index on j"}' */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci /*!80021 ENGINE_ATTRIBUTE='{"table attr": "t1"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"secondary table attr": "t1"}' */
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	{"table attr": "t1"}	{"secondary table attr": "t1"}
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1' ORDER BY column_name;
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	{"column attr": "i"}	{"secondary column attr": "i"}
def	test	t1	j	{"column attr": "j"}	{"secondary column attr": "j"}
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1' ORDER BY constraint_name;
CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	PRIMARY	t1	NULL	NULL
def	test	jix	t1	{"index attr": "index on j"}	{"secondary index attr": "index on j"}
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
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL,
  `j` int DEFAULT NULL /*!80021 ENGINE_ATTRIBUTE '{"column attr": "j"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary column attr": "j"}' */,
  KEY `jix` (`i`) /*!80021 ENGINE_ATTRIBUTE '{"index attr": "index on j"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE '{"secondary index attr": "index on j"}' */
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='A comment' /*!80021 ENGINE_ATTRIBUTE='{"table attr": "t1"}' */ /*!80021 SECONDARY_ENGINE_ATTRIBUTE='{"secondary table attr": "t1"}' */
SELECT * FROM information_schema.tables_extensions WHERE table_name = 't1';
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	{"table attr": "t1"}	{"secondary table attr": "t1"}
SELECT * FROM information_schema.columns_extensions WHERE table_name = 't1' ORDER BY column_name;
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	t1	i	NULL	NULL
def	test	t1	j	{"column attr": "j"}	{"secondary column attr": "j"}
SELECT * FROM information_schema.table_constraints_extensions WHERE table_name = 't1' ORDER BY constraint_name;
CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE
def	test	jix	t1	{"index attr": "index on j"}	{"secondary index attr": "index on j"}
DROP TABLE t1;
# Verfy that handler::check_if_supported_inplace_alter
# (default if not overridden by SE) behaves the same way for
# engine attributes as for comments.
CREATE TABLE t1(i INT, j INT) ENGINE=MYISAM;
ALTER TABLE t1 COMMENT 'A comment', ALGORITHM=INSTANT;
ALTER TABLE t1 ENGINE_ATTRIBUTE='{"table algo": "inplace"}',
ALGORITHM=INSTANT;
ALTER TABLE t1 MODIFY COLUMN i INT COMMENT 'Modified column with comment', ALGORITHM=INSTANT;
ALTER TABLE t1 MODIFY COLUMN i INT ENGINE_ATTRIBUTE='{"modify column algo": "inplace"}', ALGORITHM=INSTANT;
ALTER TABLE t1 CHANGE COLUMN i mm INT COMMENT 'Changed column with comment', ALGORITHM=INSTANT;
ALTER TABLE t1 CHANGE COLUMN j nn INT ENGINE_ATTRIBUTE='{"change column algo": "inplace" }', ALGORITHM=INSTANT;
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
# main.engine_attribute_debug END
