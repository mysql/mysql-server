CREATE TEMPORARY TABLE t1 (f1 int, f2 int primary key, UNIQUE KEY (f1));
SHOW COLUMNS FROM t1;
Field	Type	Null	Key	Default	Extra
f1	int	YES	UNI	NULL	NULL
f2	int	NO	PRI	NULL	NULL
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	0	PRIMARY	1	f2	A	0	NULL	NULL		BTREE			YES	NULL
t1	0	f1	1	f1	A	0	NULL	NULL	YES	BTREE			YES	NULL
SELECT * FROM information_schema.tmp_tables_columns;
ERROR 42S02: Unknown table 'TMP_TABLES_COLUMNS' in information_schema
SELECT * FROM information_schema.tmp_tables_keys;
ERROR 42S02: Unknown table 'TMP_TABLES_KEYS' in information_schema
DROP TEMPORARY TABLE t1;
CREATE TABLE t1 (f1 int);
SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE
FROM information_schema.tables WHERE table_name='t1';
TABLE_SCHEMA	TABLE_NAME	TABLE_TYPE
test	t1	BASE TABLE
LOCK TABLE t1 READ;
SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE
FROM information_schema.tables WHERE table_name='t1';
TABLE_SCHEMA	TABLE_NAME	TABLE_TYPE
test	t1	BASE TABLE
PREPARE st2 FROM
"SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE FROM
   information_schema.tables WHERE table_name='t1'";
EXECUTE st2;
TABLE_SCHEMA	TABLE_NAME	TABLE_TYPE
test	t1	BASE TABLE
DEALLOCATE PREPARE st2;
UNLOCK TABLES;
DROP TABLE t1;
CREATE DATABASE abc;
CREATE TABLE abc.memorytable (id INT NOT NULL) ENGINE=MEMORY;
# restart
SELECT * FROM information_schema.TABLES WHERE TABLE_SCHEMA = 'abc';
DROP DATABASE abc;
#
# Bug#30769965
# duplicate entry for key 'primary' when querying
# information_schema.tables
#
# When we run I_S query, we attempt to update the table
# 'mysql.table_stats'.  There exists no entry in mysql.table_stats
# for table db1.t1, when I_S query started execution. Once the I_S
# query starts execution it sees only the snapshot image of
# mysql.table_stats. Meanwhile I_S query is still executing, there
# can be a thread that creates a table db1.t1. The I_S query being
# executed first would attempt to update the dynamic statistics for
# db1.t1.  This can lead to DUPLICATE KEY error.
#
# Case 1: Test using InnoDB table.
#
CREATE TABLE t0 (c0 INT);
INSERT INTO t0 VALUES (1),(2),(3),(4),(5);
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
# Wait just before inserting dynamic statistics for t0.
SET DEBUG_SYNC='before_insert_into_dd SIGNAL parked WAIT_FOR cont1';
SELECT TABLE_NAME, ENGINE, TABLE_ROWS
INTO @v1, @v2, @v3
FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';
# In another connection, query I_S.TABLES to cause
# dynamic statistics to be inserted before con1.
SET DEBUG_SYNC= 'now WAIT_FOR parked';
SELECT TABLE_NAME, ENGINE, TABLE_ROWS
INTO @v1, @v2, @v3
FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';
SET DEBUG_SYNC= 'now SIGNAL cont1';
SELECT TABLE_NAME, ENGINE, TABLE_ROWS
INTO @v1, @v2, @v3
FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';
# Cleanup
DROP TABLE t0;
SET GLOBAL DEBUG=DEFAULT;
SET DEBUG_SYNC = 'RESET';
#
# Case 2: Testing MYISAM table path.
#
CREATE TABLE t0 (c0 INT) ENGINE=MyISAM;
INSERT INTO t0 VALUES (1),(2),(3),(4),(5);
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
# Wait just before inserting dynamic statistics for t0.
SET DEBUG_SYNC='before_insert_into_dd SIGNAL parked WAIT_FOR cont1';
SELECT TABLE_NAME, ENGINE, TABLE_ROWS
INTO @v1, @v2, @v3
FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';
# In another connection, query I_S.TABLES to cause
# dynamic statistics to be inserted before con1.
SET DEBUG_SYNC= 'now WAIT_FOR parked';
SELECT TABLE_NAME, ENGINE, TABLE_ROWS
INTO @v1, @v2, @v3
FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';
SET DEBUG_SYNC= 'now SIGNAL cont1';
SELECT TABLE_NAME, ENGINE, TABLE_ROWS
INTO @v1, @v2, @v3
FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';
# Cleanup
DROP TABLE t0;
SET GLOBAL DEBUG=DEFAULT;
SET DEBUG_SYNC = 'RESET';
#
# Case 3: Test Case1 with ANALYZE TABLE and a I_S query running in
#         parallel. This is for Bug#31582758.
#
CREATE TABLE t0 (c0 INT);
INSERT INTO t0 VALUES (1),(2),(3),(4),(5);
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
# Wait just before inserting dynamic statistics for t0.
SET DEBUG_SYNC='before_insert_into_dd SIGNAL parked WAIT_FOR cont1';
ANALYZE TABLE t0;
# In another connection, query I_S.TABLES to cause
# dynamic statistics to be inserted before con1.
SET DEBUG_SYNC= 'now WAIT_FOR parked';
SELECT TABLE_NAME, ENGINE, TABLE_ROWS
INTO @v1, @v2, @v3
FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';
SET DEBUG_SYNC= 'now SIGNAL cont1';
Table	Op	Msg_type	Msg_text
test.t0	analyze	status	OK
SELECT TABLE_NAME, ENGINE, TABLE_ROWS
INTO @v1, @v2, @v3
FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';
# Cleanup
DROP TABLE t0;
SET GLOBAL DEBUG=DEFAULT;
SET DEBUG_SYNC = 'RESET';
#
# Bug#33538106: There is a problem when calling the system table
# in stored function
#
# Verify that dynamic information from I_S reflects all changes also when
# queried from a stored function or prepared statement.
CREATE TABLE t1 (
org_id INT NOT NULL AUTO_INCREMENT,
org_code VARCHAR(16) NOT NULL,
PRIMARY KEY (org_id));
INSERT INTO t1 VALUES (NULL, '1');
INSERT INTO t1 VALUES (NULL, '2');
SET SESSION information_schema_stats_expiry = 0;
CREATE FUNCTION f1(table_name VARCHAR(64)) RETURNS INT
BEGIN
DECLARE dbname VARCHAR(32) DEFAULT 'test';
DECLARE nextid INT;
SELECT DATABASE() INTO dbname;
SELECT MAX(AUTO_INCREMENT) INTO nextid FROM information_schema.tables t WHERE
table_schema = dbname AND t.table_name = table_name;
RETURN nextid;
END;
/
PREPARE ps1 FROM 'SELECT MAX(AUTO_INCREMENT) = ? FROM information_schema.tables WHERE
table_schema = ? AND table_name = ?';
# Expect the value to be 3
SELECT MAX(AUTO_INCREMENT) FROM information_schema.tables t WHERE
table_schema = 'test' AND t.table_name = 't1';
MAX(AUTO_INCREMENT)
3
SELECT f1('t1') = 3;
f1('t1') = 3
1
SET @expected = 3;
SET @db = 'test';
SET @table = 't1';
EXECUTE ps1 USING @expected, @db, @table;
MAX(AUTO_INCREMENT) = ?
1
INSERT INTO t1 VALUES (NULL, '1');
# After additional insert the expected value is 4
SELECT MAX(AUTO_INCREMENT) FROM information_schema.tables t WHERE
table_schema = 'test' AND t.table_name = 't1';
MAX(AUTO_INCREMENT)
4
# When using a function
SELECT f1('t1') = 4;
f1('t1') = 4
1
# And also with a prepared statement
SET @expected = 4;
EXECUTE ps1 USING @expected, @db, @table;
MAX(AUTO_INCREMENT) = ?
1
# Testing caching of I_S.FILES
CREATE TABLE t2 (b CHAR(250), c CHAR(250));
# Procedure querying dynamic columns from I_S.FILES
CREATE PROCEDURE p2(tablespace VARCHAR(64))
BEGIN
SELECT DATA_FREE, FREE_EXTENTS, TOTAL_EXTENTS FROM information_schema.files WHERE
TABLESPACE_NAME = tablespace;
END;
/
# Convenince procedure to populate table with data to make dyanmic
# columns change
CREATE PROCEDURE populate_t2()
BEGIN
DECLARE i INT DEFAULT 1;
WHILE (i <= 6000) DO
INSERT INTO t2 (b,c) VALUES (repeat('b', 250), repeat('c', 250));
SET i = i + 1;
END WHILE;
END; /
PREPARE ps2 FROM 'SELECT DATA_FREE, FREE_EXTENTS, TOTAL_EXTENTS FROM information_schema.files WHERE
TABLESPACE_NAME = ?';
# Query I_S for empty table
CALL p2('test/t2');
DATA_FREE	FREE_EXTENTS	TOTAL_EXTENTS
0	0	0
SET @tablespace = 'test/t2';
EXECUTE ps2 USING @tablepspace;
DATA_FREE	FREE_EXTENTS	TOTAL_EXTENTS
CALL populate_t2();
# Querying I_S after table has been populated must show updated values
CALL p2('test/t2');
DATA_FREE	FREE_EXTENTS	TOTAL_EXTENTS
4194304	2	11
EXECUTE ps2 USING @tablespace;
DATA_FREE	FREE_EXTENTS	TOTAL_EXTENTS
4194304	2	11
SELECT DATA_FREE, FREE_EXTENTS, TOTAL_EXTENTS FROM information_schema.files WHERE
TABLESPACE_NAME = 'test/t2';
DATA_FREE	FREE_EXTENTS	TOTAL_EXTENTS
4194304	2	11
# Cleanup
DROP PROCEDURE populate_t2;
DROP PROCEDURE p2;
DROP TABLE t2;
DEALLOCATE PREPARE ps1;
DROP FUNCTION f1;
DROP TABLE t1;
SET SESSION information_schema_stats_expiry = default;
