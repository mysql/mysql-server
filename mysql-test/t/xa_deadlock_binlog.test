--source include/have_binlog_format_row.inc
--source include/have_log_bin.inc
--source include/rpl/init_source_replica.inc

--echo #
--echo # BUG#32707060: INCONSISTENT BINLOG CAUSES ON REPLICA ERROR 'XAER_RMFAIL:
--echo #               THE COMMAND CANNOT BE EXECUTED WHEN GLOBAL TRANSACTION IS
--echo #               IN THE ACTIVE STATE'
--echo #

CREATE TABLE t1 (c1 INT AUTO_INCREMENT PRIMARY KEY, c2 VARCHAR(5));
INSERT INTO t1(c2) VALUES("a");
INSERT INTO t1(c2) VALUES("b");

XA START 'xa1_t1';
SELECT * FROM t1 WHERE c1 = 2 FOR UPDATE;
UPDATE t1 SET c2 = 'c' WHERE c1 = 2;

--source include/rpl/connection_source1.inc
XA START 'xa2_t1';
SELECT * FROM t1 WHERE c1 = 1 FOR UPDATE;
UPDATE t1 SET c2 = 'd' WHERE c1 = 1;

--source include/rpl/connection_source.inc
# This will wait for lock until "xa2_t1" triggers deadlock and gets rejected.
--send SELECT * FROM t1 WHERE c1 = 1 FOR UPDATE

--source include/rpl/connection_source1.inc
# This will result in a rejected transaction due to detection of deadlock condition.
--sleep 1
--error ER_LOCK_DEADLOCK
SELECT * FROM t1 WHERE c1 = 2 FOR UPDATE;

--source include/rpl/connection_source.inc
--reap
UPDATE t1 SET c2 = 'e' WHERE c1 = 1;
XA END 'xa1_t1';
XA PREPARE 'xa1_t1';
XA COMMIT 'xa1_t1';

--source include/rpl/connection_source1.inc
# These statements will report an error because transaction xa2_t1 accepts only
# XA ROLLBACK.
--error ER_XA_RBDEADLOCK
XA END 'xa2_t1';
--error ER_XA_RBDEADLOCK
UPDATE t1 SET c2 = 'f' WHERE c1 = 2;
--error ER_XA_RBDEADLOCK
SELECT c1 FROM t1;
--error ER_XA_RBDEADLOCK
XA END 'xa2_t1';

# Successfully rollbacks the transaction.
XA ROLLBACK 'xa2_t1';

# Assuring that the table holds the right data in its fields.
--let $assert_text= Incorrect content in table t1. c2 should contain 'e'
--let $assert_cond= "[SELECT c2 FROM t1 WHERE c1 = 1]" = \'e\'
--source include/assert.inc
--let $assert_text= Incorrect content in table t1. c2 should contain 'c'
--let $assert_cond= "[SELECT c2 FROM t1 WHERE c1 = 2]" = \'c\'
--source include/assert.inc

--sync_slave_with_master
--source include/rpl/connection_source.inc
--let $binlog_file= LAST
--source include/rpl/deprecated/show_binlog_events.inc

--source include/rpl/connection_source1.inc
CREATE TABLE t2 (c1 int);

--source include/rpl/connection_source.inc
DROP TABLE t1, t2;
--source include/rpl/deinit.inc
