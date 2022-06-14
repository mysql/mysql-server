CREATE TABLE t0 (f1 INT PRIMARY KEY);
INSERT INTO t0 VALUES (1),(2),(3),(4);
#
# CASE 1
# Crash DDL after table is created but before INSERT.
#
SET global DEBUG='+d, crash_before_create_select_insert';
CREATE TABLE t1 AS SELECT * FROM t0;
ERROR HY000: Lost connection to MySQL server during query
# Recover the server.
include/assert.inc [Verify that only table t0 is present in test database]
SET global DEBUG='-d, crash_before_create_select_insert';
#
# CASE 2
# Crash DDL after table is created and INSERT is completed.
#
SET global DEBUG='+d, crash_after_create_select_insert';
CREATE TABLE t1 AS SELECT * FROM t0;
ERROR HY000: Lost connection to MySQL server during query
# Recover the server.
include/assert.inc [Verify that only table t0 is present in test database]
SET global DEBUG='-d, crash_after_create_select_insert';
#
# CASE 3
# Crash DDL during commit before flushing binlog.
#
SET global DEBUG='+d, crash_commit_before_log';
CREATE TABLE t1 AS SELECT * FROM t0;
ERROR HY000: Lost connection to MySQL server during query
# Recover the server.
include/assert.inc [Verify that only table t0 is present in test database]
SET global DEBUG='-d, crash_commit_before_log';
#
# CASE 4
# Crash DDL during commit after binlog flush.
#
SET global DEBUG='+d, crash_after_flush_binlog';
CREATE TABLE t1 AS SELECT * FROM t0;
ERROR HY000: Lost connection to MySQL server during query
# Recover the server.
include/assert.inc [Verify that only table t0 and t1 is present in test database]
SET global DEBUG='-d, crash_after_flush_binlog';
DROP TABLE t1;
#
# CASE 5
# Concurrent access to table being created should be blocked.
#
SET DEBUG_SYNC='ha_commit_trans_before_acquire_commit_lock SIGNAL cond1 WAIT_FOR cond2';
CREATE TABLE t1 AS SELECT * FROM t0;;
SET DEBUG_SYNC='now WAIT_FOR cond1';
SELECT * FROM t1;
SET DEBUG_SYNC='now SIGNAL cond2';
f1
1
2
3
4
SET DEBUG_SYNC=RESET;
DROP TABLE t0, t1;
#
# CASE 6 ROLLBACK after CREATE TABLE ... START TRANSACTION.
#
CREATE TABLE t1 (f1 INT) START TRANSACTION;
ROLLBACK;
include/assert.inc [Verify that only we don't see and table in test database]
#
# CASE 7 COMMIT after CREATE TABLE ... START TRANSACTION.
#
CREATE TABLE t1 (f1 INT) START TRANSACTION;
COMMIT;
include/assert.inc [Verify that table t1 is present in test database]
DROP TABLE t1;
#
# CASE 8 Test previous two case from within a SP.
#
CREATE PROCEDURE proc1()
BEGIN
CREATE TABLE t1 (f1 INT) START TRANSACTION;
ROLLBACK;
CREATE TABLE t1 (f1 INT) START TRANSACTION;
COMMIT;
END|
CALL proc1();
include/assert.inc [Verify that table t1 is present in test database]
# Rerun the proceduce and check for table exist error.
CALL proc1();
ERROR 42S01: Table 't1' already exists
DROP TABLE t1;
DROP PROCEDURE proc1;
#
# CASE 9 Reject prepared stmt for CREATE TABLE ... START TRANSACTION.
#
PREPARE stmt FROM "CREATE TABLE t1 (f1 INT) START TRANSACTION";
ERROR HY000: This command is not supported in the prepared statement protocol yet
#
# CASE 10
# CREATE TABLE ... START TRANSACTION with SE not supporting atomic-DDL
#
CREATE TABLE t1 (f1 INT) ENGINE=MyiSAM START TRANSACTION;
ERROR HY000: START TRANSACTION clause cannot be used with engine that does not support atomic DDL.
#
# CASE 11
# Reject DML, DDL and other commands except for COMMIT, ROLLBACK after
# CREATE TABLE ... START TRANSACTION.
#
CREATE TABLE t1 (f1 INT) START TRANSACTION;
INSERT INTO t1 VALUES (1);
ERROR HY000: Only BINLOG INSERT, COMMIT and ROLLBACK statements are allowed after CREATE TABLE with START TRANSACTION statement.
UPDATE t1 SET f1=932;
ERROR HY000: Only BINLOG INSERT, COMMIT and ROLLBACK statements are allowed after CREATE TABLE with START TRANSACTION statement.
CREATE TABLE t2 (f2 INT);
ERROR HY000: Only BINLOG INSERT, COMMIT and ROLLBACK statements are allowed after CREATE TABLE with START TRANSACTION statement.
SET sql_mode = default;
ERROR HY000: Only BINLOG INSERT, COMMIT and ROLLBACK statements are allowed after CREATE TABLE with START TRANSACTION statement.
ROLLBACK;
include/assert.inc [Verify that no table exists in test database]
#
# CASE 12
# ALTER TABLE ... START TRANSACTION is not supported.
#
CREATE TABLE t1 (f1 INT);
ALTER TABLE t2 ADD f2 INT, START TRANSACTION;
ERROR HY000: START TRANSACTION clause cannot be used with ALTER TABLE command.
DROP TABLE t1;
#
# CASE 13
# CREATE TEMPORARY TABLE ... START TRANSACTION is not supported.
#
CREATE TEMPORARY TABLE t1 (f1 INT) START TRANSACTION;
ERROR HY000: START TRANSACTION clause cannot be used to create temporary tables.
#
# CASE 14
# CREATE TABLE ... AS SELECT with START TRANSACTION.
#
CREATE TABLE t1 START TRANSACTION as SELECT * FROM t0;
ERROR HY000: START TRANSACTION clause cannot be used with CREATE TABLE ... AS SELECT statement.
