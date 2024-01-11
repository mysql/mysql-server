#########################################
# ==== Purpose ====
#
# Test crash and recovery of CREATE TABLE ... AS SELECT (DDL_CTAS)
# and also test CREATE TABLE ... START TRANSACTION and related cases.
#
# ==== Requirements ====
#
# R1 Crash before committing DDL_CTAS should cause metadata of table
#    being created to be rollbacked.
#
# R2 CREATE TABLE ... TRANSACTION; command should not allow any SQL
#    commands other than BINLOG INSERT, COMMIT and ROLLBACK.
#
# R3 Reject use of CREATE TABLE ... TRANSACTION using a prepared statement.
#
# ==== Implementation ====
#
# TC1: Crash DDL_CTAS after table is created but before INSERT.
# 1) Create table t0 with few rows.
# 2) Set debug point to induce crash before inserting rows during DDL_CTAS.
#    crash_before_create_select_insert
# 3) Execute DDL_CTAS and cause crash.
# 4) Wait for server to stop and then restart the server.
# 5) Verify that we have just t0 and no t1 created.
# 6) Reset the debug point.
#
# TC2: Crash DDL_CTAS after table is created and INSERT is completed.
# Repeat steps from TC1 with debug point crash_after_create_select_insert.
#
# TC3: Crash DDL_CTAS during commit before flushing binlog.
# Repeat steps from TC1 with debug point crash_commit_before_log.
#
# TC4: Crash DDL_CTAS during commit after flushing binlog.
# Repeat steps from TC1 with debug point crash_after_flush_binlog.
#
# TC5: Concurrent access to table being created should be blocked.
# 1) Execute CREATE TABLE ... START TRANSACTION; and block during commit;
# 2) Execute SELECT * FROM t1; in another connection;
# 3) Verify that SELECT command is waiting for MDL lock.
# 4) Continue execution of 1).
# 5) Verify that we see results from SELECT.
#
# TC6: Test ROLLBACK after CREATE TABLE ... START TRANSACTION.
# 1) Execute CREATE TABLE ... START TRANSACTION;
# 2) Execute ROLLBACK;
# 3) Verify that table t1 does not exist.
#
# TC7: Test COMMIT after CREATE TABLE ... START TRANSACTION.
# 1) Execute CREATE TABLE ... START TRANSACTION;
# 2) Execute COMMIT;
# 3) Verify that table t1 does exist.
#
# TC8: Test previous two cases from within a SP.
# 1) Create a procedure with following steps.
#    - Steps 1/2 in TC5.
#    - Steps 1/2 in TC6.
# 2) Execute the procedure.
# 3) Verify that table t1 does exist.
#
# TC9: Reject prepared statement and CREATE TABLE .. START TRANSACTION.
# 1) Test that we get ER_UNSUPPORTED_PS if CREATE TABLE ... START
#    TRANSACTION is executed using PREPARE command.
#
# TC10: Reject CREATE TABLE .. START TRANSACTION with non-atomic engine.
# 1) Test that we get ER_NOT_ALLOWED_WITH_START_TRANSACTION if CREATE
#    TABLE ... START TRANSACTION is executed using SE does not support
#    atomic DDL.
#
# TC11: Reject DML, DDL and other commands except for COMMIT, ROLLBACK after
#      CREATE TABLE ... START TRANSACTION.
# 1) Execute CREATE TABLE ... START TRANSACTION;
# 2) Execute INSERT and see we get
#    ER_STATEMENT_NOT_ALLOWED_AFTER_START_TRANSACTION
# 3) Execute UPDATE and see we get
#    ER_STATEMENT_NOT_ALLOWED_AFTER_START_TRANSACTION
# 4) Execute SET and see we get
#    ER_STATEMENT_NOT_ALLOWED_AFTER_START_TRANSACTION
#
# TC12: Reject ALTER TABLE with START TRANSACTION.
#
# TC13: Reject CREATE TEMPORARY TABLE with START TRANSACTION.
#
# TC14: Reject CREATE TABLE ... AS SELECT with START TRANSACTION.
#
# ==== References ====
#
# WL#13355 Make CREATE TABLE...SELECT atomic and crash-safe
#

--source include/have_debug.inc
--source include/not_valgrind.inc
--source include/have_log_bin.inc

# Skip ps protocol because CREATE TABLE ... START TRANSACTION is not
# allowed to be run with ps protocol.
--source include/no_ps_protocol.inc

CREATE TABLE t0 (f1 INT PRIMARY KEY);
INSERT INTO t0 VALUES (1),(2),(3),(4);

--echo #
--echo # CASE 1
--echo # Crash DDL after table is created but before INSERT.
--echo #

--source include/expect_crash.inc
SET global DEBUG='+d, crash_before_create_select_insert';
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--error CR_SERVER_LOST
CREATE TABLE t1 AS SELECT * FROM t0;

--echo # Recover the server.
--source include/wait_until_disconnected.inc
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
let $WAIT_COUNT=6000;
--source include/wait_time_until_connected_again.inc

--let $assert_text= Verify that only table t0 is present in test database
let $assert_cond= [SELECT count(table_name) COUNT FROM
  INFORMATION_SCHEMA.TABLES WHERE table_schema = \'test\', COUNT, 1] = 1;
--source include/assert.inc

SET global DEBUG='-d, crash_before_create_select_insert';

--echo #
--echo # CASE 2
--echo # Crash DDL after table is created and INSERT is completed.
--echo #

--source include/expect_crash.inc
SET global DEBUG='+d, crash_after_create_select_insert';
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--error CR_SERVER_LOST
CREATE TABLE t1 AS SELECT * FROM t0;

--echo # Recover the server.
--source include/wait_until_disconnected.inc
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
let $WAIT_COUNT=6000;
--source include/wait_time_until_connected_again.inc

--let $assert_text= Verify that only table t0 is present in test database
--let $assert_cond= "[SELECT count(table_name) COUNT FROM INFORMATION_SCHEMA.TABLES WHERE table_schema = \'test\', COUNT, 1]" = "1"
--source include/assert.inc

SET global DEBUG='-d, crash_after_create_select_insert';

--echo #
--echo # CASE 3
--echo # Crash DDL during commit before flushing binlog.
--echo #

--source include/expect_crash.inc
SET global DEBUG='+d, crash_commit_before_log';
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--error CR_SERVER_LOST
CREATE TABLE t1 AS SELECT * FROM t0;

--echo # Recover the server.
--source include/wait_until_disconnected.inc
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
let $WAIT_COUNT=6000;
--source include/wait_time_until_connected_again.inc

--let $assert_text= Verify that only table t0 is present in test database
let $assert_cond= [SELECT count(table_name) COUNT FROM
  INFORMATION_SCHEMA.TABLES WHERE table_schema = \'test\', COUNT, 1] = 1;
--source include/assert.inc

SET global DEBUG='-d, crash_commit_before_log';

--echo #
--echo # CASE 4
--echo # Crash DDL during commit after binlog flush.
--echo #

--source include/expect_crash.inc
SET global DEBUG='+d, crash_after_flush_binlog';
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--error CR_SERVER_LOST
CREATE TABLE t1 AS SELECT * FROM t0;

--echo # Recover the server.
--source include/wait_until_disconnected.inc
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
let $WAIT_COUNT=6000;
--source include/wait_time_until_connected_again.inc

--let $assert_text= Verify that only table t0 and t1 is present in test database
--let $assert_cond= "[SELECT count(table_name) COUNT FROM INFORMATION_SCHEMA.TABLES WHERE table_schema = \'test\', COUNT, 1]" = "2"
--source include/assert.inc

SET global DEBUG='-d, crash_after_flush_binlog';
DROP TABLE t1;

--echo #
--echo # CASE 5
--echo # Concurrent access to table being created should be blocked.
--echo #

--connection default
SET DEBUG_SYNC='ha_commit_trans_before_acquire_commit_lock SIGNAL cond1 WAIT_FOR cond2';
--send CREATE TABLE t1 AS SELECT * FROM t0;

--connect (con1, localhost, root,,)
SET DEBUG_SYNC='now WAIT_FOR cond1';
--send SELECT * FROM t1

--connect (con2, localhost, root,,)
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "SELECT * FROM t1";
--source include/wait_condition.inc
SET DEBUG_SYNC='now SIGNAL cond2';

--connection default
--reap
--connection con1
--reap
--connection default
--disconnect con1
--disconnect con2

SET DEBUG_SYNC=RESET;
DROP TABLE t0, t1;

--echo #
--echo # CASE 6 ROLLBACK after CREATE TABLE ... START TRANSACTION.
--echo #
CREATE TABLE t1 (f1 INT) START TRANSACTION;
ROLLBACK;
--let $assert_text= Verify that only we don't see and table in test database
--let $assert_cond= "[SELECT count(table_name) COUNT FROM INFORMATION_SCHEMA.TABLES WHERE table_schema = \'test\', COUNT, 1]" = "0"
--source include/assert.inc

--echo #
--echo # CASE 7 COMMIT after CREATE TABLE ... START TRANSACTION.
--echo #
CREATE TABLE t1 (f1 INT) START TRANSACTION;
COMMIT;
--let $assert_text= Verify that table t1 is present in test database
--let $assert_cond= "[SELECT count(table_name) COUNT FROM INFORMATION_SCHEMA.TABLES WHERE table_schema = \'test\', COUNT, 1]" = "1"
--source include/assert.inc

DROP TABLE t1;

--echo #
--echo # CASE 8 Test previous two case from within a SP.
--echo #
DELIMITER |;
CREATE PROCEDURE proc1()
BEGIN
  CREATE TABLE t1 (f1 INT) START TRANSACTION;
  ROLLBACK;
  CREATE TABLE t1 (f1 INT) START TRANSACTION;
  COMMIT;
END|
DELIMITER ;|
CALL proc1();
--let $assert_text= Verify that table t1 is present in test database
--let $assert_cond= "[SELECT count(table_name) COUNT FROM INFORMATION_SCHEMA.TABLES WHERE table_schema = \'test\', COUNT, 1]" = "1"
--source include/assert.inc
--echo # Rerun the proceduce and check for table exist error.
--error ER_TABLE_EXISTS_ERROR
CALL proc1();
DROP TABLE t1;
DROP PROCEDURE proc1;

--echo #
--echo # CASE 9 Reject prepared stmt for CREATE TABLE ... START TRANSACTION.
--echo #
--error ER_UNSUPPORTED_PS
PREPARE stmt FROM "CREATE TABLE t1 (f1 INT) START TRANSACTION";

--echo #
--echo # CASE 10
--echo # CREATE TABLE ... START TRANSACTION with SE not supporting atomic-DDL
--echo #
--error ER_NOT_ALLOWED_WITH_START_TRANSACTION
CREATE TABLE t1 (f1 INT) ENGINE=MyiSAM START TRANSACTION;

--echo #
--echo # CASE 11
--echo # Reject DML, DDL and other commands except for COMMIT, ROLLBACK after
--echo # CREATE TABLE ... START TRANSACTION.
--echo #
CREATE TABLE t1 (f1 INT) START TRANSACTION;
--error ER_STATEMENT_NOT_ALLOWED_AFTER_START_TRANSACTION
INSERT INTO t1 VALUES (1);
--error ER_STATEMENT_NOT_ALLOWED_AFTER_START_TRANSACTION
UPDATE t1 SET f1=932;
--error ER_STATEMENT_NOT_ALLOWED_AFTER_START_TRANSACTION
CREATE TABLE t2 (f2 INT);
--error ER_STATEMENT_NOT_ALLOWED_AFTER_START_TRANSACTION
SET sql_mode = default;
ROLLBACK;

--let $assert_text= Verify that no table exists in test database
--let $assert_cond= "[SELECT count(table_name) COUNT FROM INFORMATION_SCHEMA.TABLES WHERE table_schema = \'test\', COUNT, 1]" = "0"
--source include/assert.inc

--echo #
--echo # CASE 12
--echo # ALTER TABLE ... START TRANSACTION is not supported.
--echo #
CREATE TABLE t1 (f1 INT);
--error ER_NOT_ALLOWED_WITH_START_TRANSACTION
ALTER TABLE t2 ADD f2 INT, START TRANSACTION;
DROP TABLE t1;

--echo #
--echo # CASE 13
--echo # CREATE TEMPORARY TABLE ... START TRANSACTION is not supported.
--echo #
--error ER_NOT_ALLOWED_WITH_START_TRANSACTION
CREATE TEMPORARY TABLE t1 (f1 INT) START TRANSACTION;

--echo #
--echo # CASE 14
--echo # CREATE TABLE ... AS SELECT with START TRANSACTION.
--echo #
--error ER_NOT_ALLOWED_WITH_START_TRANSACTION
CREATE TABLE t1 START TRANSACTION as SELECT * FROM t0;
