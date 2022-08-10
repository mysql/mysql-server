--source include/have_debug.inc
--source include/have_debug_sync.inc

# 
# Bug#23209797 SEGMENTATION FAULT WHILE GETTING GET_SCHEMA_TABLES_RESULT()
# Make sure information_schema.tmp_tables_* tables are not
# directly accessible to users, except for SHOW COMMANDS.
#

CREATE TEMPORARY TABLE t1 (f1 int, f2 int primary key, UNIQUE KEY (f1));
SHOW COLUMNS FROM t1;
SHOW INDEXES FROM t1;
--error ER_UNKNOWN_TABLE
SELECT * FROM information_schema.tmp_tables_columns;
--error ER_UNKNOWN_TABLE
SELECT * FROM information_schema.tmp_tables_keys;
DROP TEMPORARY TABLE t1;


#
# Bug#23210930 ASSERTION `THD->GET_TRANSACTION()->IS_EMPTY(TRANSACTION_CTX::STMT)' FAILED.
# Make sure the INFORMATION_SCHEMA system views are usable in
# prepared statement.
#

CREATE TABLE t1 (f1 int);
SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE
  FROM information_schema.tables WHERE table_name='t1';

LOCK TABLE t1 READ;
SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE
   FROM information_schema.tables WHERE table_name='t1';
PREPARE st2 FROM
  "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE FROM
   information_schema.tables WHERE table_name='t1'";
EXECUTE st2;
DEALLOCATE PREPARE st2;
UNLOCK TABLES;

DROP TABLE t1;

#
# Bug #28165060   MYSQL IS TRYING TO PERFORM A CONSISTENT READ BUT THE READ
# VIEW IS NOT ASSIGNED!
#

CREATE DATABASE abc;
CREATE TABLE abc.memorytable (id INT NOT NULL) ENGINE=MEMORY;
--source include/restart_mysqld.inc
--disable_result_log
# The following command would cause a assert, without the fix.
SELECT * FROM information_schema.TABLES WHERE TABLE_SCHEMA = 'abc';
--enable_result_log
DROP DATABASE abc;

--echo #
--echo # Bug#30769965
--echo # duplicate entry for key 'primary' when querying
--echo # information_schema.tables
--echo #
--echo # When we run I_S query, we attempt to update the table
--echo # 'mysql.table_stats'.  There exists no entry in mysql.table_stats
--echo # for table db1.t1, when I_S query started execution. Once the I_S
--echo # query starts execution it sees only the snapshot image of
--echo # mysql.table_stats. Meanwhile I_S query is still executing, there
--echo # can be a thread that creates a table db1.t1. The I_S query being
--echo # executed first would attempt to update the dynamic statistics for
--echo # db1.t1.  This can lead to DUPLICATE KEY error.

--echo #
--echo # Case 1: Test using InnoDB table.
--echo #

CREATE TABLE t0 (c0 INT);
INSERT INTO t0 VALUES (1),(2),(3),(4),(5);
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;

--echo # Wait just before inserting dynamic statistics for t0.
connect (con1, localhost, root,,);
SET DEBUG_SYNC='before_insert_into_dd SIGNAL parked WAIT_FOR cont1';
send SELECT TABLE_NAME, ENGINE, TABLE_ROWS
       INTO @v1, @v2, @v3
       FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';

--echo # In another connection, query I_S.TABLES to cause
--echo # dynamic statistics to be inserted before con1.
connection default;
SET DEBUG_SYNC= 'now WAIT_FOR parked';
SELECT TABLE_NAME, ENGINE, TABLE_ROWS
  INTO @v1, @v2, @v3
  FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';

SET DEBUG_SYNC= 'now SIGNAL cont1';

connection con1;
# Without this fix we would have expected a ER_DUP_ENTRY warning here.
--reap

# Verify that we have latest statistics stored.
SELECT TABLE_NAME, ENGINE, TABLE_ROWS
  INTO @v1, @v2, @v3
  FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';

--echo # Cleanup
connection default;
DROP TABLE t0;
disconnect con1;
SET GLOBAL DEBUG=DEFAULT;
SET DEBUG_SYNC = 'RESET';

--echo #
--echo # Case 2: Testing MYISAM table path.
--echo #

CREATE TABLE t0 (c0 INT) ENGINE=MyISAM;
INSERT INTO t0 VALUES (1),(2),(3),(4),(5);
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;

--echo # Wait just before inserting dynamic statistics for t0.
connect (con1, localhost, root,,);
SET DEBUG_SYNC='before_insert_into_dd SIGNAL parked WAIT_FOR cont1';
send SELECT TABLE_NAME, ENGINE, TABLE_ROWS
       INTO @v1, @v2, @v3
       FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';

--echo # In another connection, query I_S.TABLES to cause
--echo # dynamic statistics to be inserted before con1.
connection default;
SET DEBUG_SYNC= 'now WAIT_FOR parked';
SELECT TABLE_NAME, ENGINE, TABLE_ROWS
  INTO @v1, @v2, @v3
  FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';

SET DEBUG_SYNC= 'now SIGNAL cont1';

connection con1;
# Without this fix we would have expected a ER_DUP_ENTRY error here.
--reap

# Verify that we have latest statistics stored.
SELECT TABLE_NAME, ENGINE, TABLE_ROWS
  INTO @v1, @v2, @v3
  FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';

--echo # Cleanup
connection default;
DROP TABLE t0;
disconnect con1;
SET GLOBAL DEBUG=DEFAULT;
SET DEBUG_SYNC = 'RESET';

--echo #
--echo # Case 3: Test Case1 with ANALYZE TABLE and a I_S query running in
--echo #         parallel. This is for Bug#31582758.
--echo #

CREATE TABLE t0 (c0 INT);
INSERT INTO t0 VALUES (1),(2),(3),(4),(5);
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;
INSERT INTO t0 SELECT * FROM t0;

--echo # Wait just before inserting dynamic statistics for t0.
connect (con1, localhost, root,,);
SET DEBUG_SYNC='before_insert_into_dd SIGNAL parked WAIT_FOR cont1';
send ANALYZE TABLE t0;

--echo # In another connection, query I_S.TABLES to cause
--echo # dynamic statistics to be inserted before con1.
connection default;
SET DEBUG_SYNC= 'now WAIT_FOR parked';
SELECT TABLE_NAME, ENGINE, TABLE_ROWS
  INTO @v1, @v2, @v3
  FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';

SET DEBUG_SYNC= 'now SIGNAL cont1';

connection con1;
# Without this fix we would have expected a ER_DUP_ENTRY warning here.
--reap

# Verify that we have latest statistics stored.
SELECT TABLE_NAME, ENGINE, TABLE_ROWS
  INTO @v1, @v2, @v3
  FROM INFORMATION_SCHEMA.TABLES WHERE table_name='t0';

--echo # Cleanup
connection default;
DROP TABLE t0;
disconnect con1;
SET GLOBAL DEBUG=DEFAULT;
SET DEBUG_SYNC = 'RESET';

--echo #
--echo # Bug#33538106: There is a problem when calling the system table
--echo # in stored function
--echo #

--echo # Verify that dynamic information from I_S reflects all changes also when
--echo # queried from a stored function or prepared statement.

CREATE TABLE t1 (
org_id INT NOT NULL AUTO_INCREMENT,
org_code VARCHAR(16) NOT NULL,
PRIMARY KEY (org_id));

INSERT INTO t1 VALUES (NULL, '1');
INSERT INTO t1 VALUES (NULL, '2');

SET SESSION information_schema_stats_expiry = 0;

DELIMITER /;

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

DELIMITER ;/

PREPARE ps1 FROM 'SELECT MAX(AUTO_INCREMENT) = ? FROM information_schema.tables WHERE
table_schema = ? AND table_name = ?';

--echo # Expect the value to be 3
SELECT MAX(AUTO_INCREMENT) FROM information_schema.tables t WHERE
table_schema = 'test' AND t.table_name = 't1';

SELECT f1('t1') = 3;

SET @expected = 3;
SET @db = 'test';
SET @table = 't1';
EXECUTE ps1 USING @expected, @db, @table;

INSERT INTO t1 VALUES (NULL, '1');

--echo # After additional insert the expected value is 4
SELECT MAX(AUTO_INCREMENT) FROM information_schema.tables t WHERE
table_schema = 'test' AND t.table_name = 't1';

--echo # When using a function
SELECT f1('t1') = 4;

--echo # And also with a prepared statement
SET @expected = 4;
EXECUTE ps1 USING @expected, @db, @table;

--echo # Testing caching of I_S.FILES
CREATE TABLE t2 (b CHAR(250), c CHAR(250));

--echo # Procedure querying dynamic columns from I_S.FILES
DELIMITER /;
CREATE PROCEDURE p2(tablespace VARCHAR(64))
BEGIN
SELECT DATA_FREE, FREE_EXTENTS, TOTAL_EXTENTS FROM information_schema.files WHERE
TABLESPACE_NAME = tablespace;
END;
/

--echo # Convenince procedure to populate table with data to make dyanmic
--echo # columns change
CREATE PROCEDURE populate_t2()
BEGIN
DECLARE i INT DEFAULT 1;
WHILE (i <= 6000) DO
  INSERT INTO t2 (b,c) VALUES (repeat('b', 250), repeat('c', 250));
  SET i = i + 1;
END WHILE;
END; /
DELIMITER ;/

PREPARE ps2 FROM 'SELECT DATA_FREE, FREE_EXTENTS, TOTAL_EXTENTS FROM information_schema.files WHERE
TABLESPACE_NAME = ?';

--echo # Query I_S for empty table
CALL p2('test/t2');
SET @tablespace = 'test/t2';
EXECUTE ps2 USING @tablepspace;

CALL populate_t2();

--echo # Querying I_S after table has been populated must show updated values
CALL p2('test/t2');
EXECUTE ps2 USING @tablespace;
SELECT DATA_FREE, FREE_EXTENTS, TOTAL_EXTENTS FROM information_schema.files WHERE
TABLESPACE_NAME = 'test/t2';

--echo # Cleanup
DROP PROCEDURE populate_t2;
DROP PROCEDURE p2;
DROP TABLE t2;
DEALLOCATE PREPARE ps1;
DROP FUNCTION f1;
DROP TABLE t1;
SET SESSION information_schema_stats_expiry = default;
