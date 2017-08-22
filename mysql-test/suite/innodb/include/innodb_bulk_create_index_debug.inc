#
# wl#7277: InnoDB: Bulk Load for Create Index
#


# This test case needs to crash the server. Needs a debug server.
-- source include/have_debug.inc

# Don't test this under valgrind, memory leaks will occur.
-- source include/not_valgrind.inc

# Avoid CrashReporter popup on Mac
-- source include/not_crashrep.inc


# Create Insert Procedure
DELIMITER |;
CREATE PROCEDURE populate_t1()
BEGIN
	DECLARE i int DEFAULT 1;

	START TRANSACTION;
	WHILE (i <= 10000) DO
		INSERT INTO t1 VALUES (i, i, CONCAT('a', i));
		SET i = i + 1;
	END WHILE;
	COMMIT;
END|
DELIMITER ;|

# Test scenarios:
# 1. Test restart;
# 2. Test crash recovery.

# Test Restart
if ($row_format != 'COMPRESSED')
{
  eval CREATE TABLE t1(
        class   INT,
        id      INT,
        title   VARCHAR(100)
  ) ENGINE=InnoDB ROW_FORMAT=$row_format;
}

if ($row_format == 'COMPRESSED')
{
  SET GLOBAL innodb_file_per_table=1;

  eval CREATE TABLE t1(
        class   INT,
        id      INT,
        title   VARCHAR(100)
  ) ENGINE=InnoDB ROW_FORMAT=$row_format KEY_BLOCK_SIZE=4;
}

-- disable_query_log
CALL populate_t1();
-- enable_query_log

SELECT COUNT(*) FROM t1;

CREATE INDEX idx_title ON t1(title);

--source include/restart_mysqld.inc

CHECK TABLE t1;

SELECT * FROM t1 WHERE title = 'a10';

SELECT * FROM t1 WHERE title = 'a5000';

SELECT * FROM t1 WHERE title = 'a10000';

SELECT * FROM t1 WHERE title = 'a10010';

DROP TABLE t1;

-- echo # Test Blob

if ($row_format != 'COMPRESSED') {
  eval CREATE TABLE t1(
        a INT PRIMARY KEY,
        b TEXT,
        c TEXT) ENGINE=InnoDB ROW_FORMAT=$row_format;
}

if ($row_format == 'COMPRESSED') {
  SET GLOBAL innodb_file_per_table=1;

  eval CREATE TABLE t1(
        a INT PRIMARY KEY,
        b TEXT,
        c TEXT) ENGINE=InnoDB ROW_FORMAT=$row_format KEY_BLOCK_SIZE=4;
}

INSERT INTO t1 VALUES
	(1, REPEAT('a',10000), 'a'),
	(2, REPEAT('b',20000), 'b'),
	(3, REPEAT('c',40000), 'c'),
	(4, REPEAT('d',60000), 'd');

SELECT CHAR_LENGTH(b) FROM t1;

ALTER TABLE t1 DROP COLUMN c;

--source include/restart_mysqld.inc

CHECK TABLE t1;

SELECT CHAR_LENGTH(b) FROM t1;

DROP TABLE t1;

# Test Crash Recovery

#
# The below part of test was disabled until InnoDB truly supports DDL
# atomicity/WL#9536 is implemented. Otherwise the test creates discrepancy
# between SQL-layer DD and InnoDB DD.
#

--disable_parsing

if ($row_format != 'COMPRESSED')
{
  eval CREATE TABLE t1(
        class   INT,
        id      INT,
        title   VARCHAR(100)
  ) ENGINE=InnoDB ROW_FORMAT=$row_format;
}

if ($row_format == 'COMPRESSED')
{
  SET GLOBAL innodb_file_per_table=1;

  eval CREATE TABLE t1(
        class   INT,
        id      INT,
        title   VARCHAR(100)
  ) ENGINE=InnoDB ROW_FORMAT=$row_format KEY_BLOCK_SIZE=4;
}

-- disable_query_log
CALL populate_t1();
-- enable_query_log

SET SESSION debug="+d,crash_after_index_create";

# Write file to make mysql-test-run.pl start up the server again
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect

--error 2013
CREATE INDEX idx_title ON t1(title);

--enable_reconnect
--source include/wait_until_connected_again.inc
--disable_reconnect

SELECT COUNT(*) FROM t1;

CHECK TABLE t1;

EXPLAIN SELECT * FROM t1 WHERE title = 'a10';

SELECT * FROM t1 WHERE title = 'a10';

SELECT * FROM t1 WHERE title = 'a5000';

SELECT * FROM t1 WHERE title = 'a10000';

SELECT * FROM t1 WHERE title = 'a10010';

DROP TABLE t1;

-- echo # Test Blob

if ($row_format != 'COMPRESSED') {
  eval CREATE TABLE t1(
        a INT PRIMARY KEY,
        b TEXT,
        c TEXT) ENGINE=InnoDB ROW_FORMAT=$row_format;
}

if ($row_format == 'COMPRESSED') {
  SET GLOBAL innodb_file_per_table=1;

  eval CREATE TABLE t1(
        a INT PRIMARY KEY,
        b TEXT,
        c TEXT) ENGINE=InnoDB ROW_FORMAT=$row_format KEY_BLOCK_SIZE=4;
}

INSERT INTO t1 VALUES
	(1, REPEAT('a',10000), 'a'),
	(2, REPEAT('b',20000), 'b'),
	(3, REPEAT('c',40000), 'c'),
	(4, REPEAT('d',60000), 'd');

SELECT CHAR_LENGTH(b) FROM t1;

SET SESSION debug="+d,crash_after_index_create";

# Write file to make mysql-test-run.pl start up the server again
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect

--error 2013
ALTER TABLE t1 DROP COLUMN c;

--enable_reconnect
--source include/wait_until_connected_again.inc
--disable_reconnect

CHECK TABLE t1;

SELECT CHAR_LENGTH(b) FROM t1;

DROP TABLE t1;

--enable_parsing

# Restore global variables
if ($row_format == 'COMPRESSED')
{
  SET GLOBAL innodb_file_per_table=default;
}

DROP PROCEDURE populate_t1;
