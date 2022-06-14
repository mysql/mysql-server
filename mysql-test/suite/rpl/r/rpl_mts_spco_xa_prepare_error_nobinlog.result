include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CALL mtr.add_suppression("Statement is unsafe because it is being used inside a XA transaction");
[connection slave]
CALL mtr.add_suppression(".*slave coordinator and worker threads are stopped.*");
CALL mtr.add_suppression("Could not execute Write_rows event on table test.*");
CALL mtr.add_suppression("Worker .* failed executing transaction");
CALL mtr.add_suppression("Error writing relay log configuration");
# Adding debug point 'rpl_fake_commit_order_deadlock_for_timestamp_100' to @@GLOBAL.debug
[connection master]

# Setup

[connection slave]
CALL mtr.add_suppression("You need to use --log-bin to make --binlog-format work");
SET @save_replica_parallel_workers= @@global.replica_parallel_workers;
SET @save_replica_parallel_type= @@global.replica_parallel_type;
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET @save_replica_preserve_commit_order= @@global.replica_preserve_commit_order;
SET GLOBAL replica_parallel_type = 'LOGICAL_CLOCK';
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET GLOBAL replica_parallel_workers= 2;
SET GLOBAL replica_preserve_commit_order= ON;
include/start_slave.inc
[connection master]
SET @save_binlog_transaction_dependency_tracking= @@global.binlog_transaction_dependency_tracking;
SET @@global.binlog_transaction_dependency_tracking = COMMIT_ORDER;
[connection master]
CREATE TABLE t1 (c1 INT PRIMARY KEY) ENGINE = InnoDB; CREATE TABLE t2 (c1 INT PRIMARY KEY) ENGINE = InnoDB;;
include/sync_slave_sql_with_master.inc

# Block slave sql applier threads

[connection slave]
BEGIN; INSERT INTO t1 VALUES (1);;

# Generate the transactions which can be applied in parallel on slave

[connection master]
# Adding debug point 'set_commit_parent_100' to @@GLOBAL.debug
INSERT INTO t1 VALUES (1); INSERT INTO t1 VALUES (2); XA START 'xa_test_xid1'; INSERT INTO t1 VALUES (3); XA END 'xa_test_xid1'; SET TIMESTAMP = 200; XA PREPARE 'xa_test_xid1';;
[connection server_1]
BEGIN; INSERT INTO t2 VALUES (1); INSERT INTO t2 VALUES (2); COMMIT;;
# Removing debug point 'set_commit_parent_100' from @@GLOBAL.debug

# Verify the transactions are ordered correctly on slave

[connection server_2]
include/assert.inc [Verify table t1 has two rows]
include/assert.inc [Verify table t2 is empty]

# Rollback the first insert so that slave applier threads can
# unblock and proceed. Verify the transactions are applied.

[connection slave]
ROLLBACK;;

# On slave, verify table t1 has only two rows which were inserted before
# XA transaction. Wait for slave sql thread to fail with ER_UNKNOWN_ERROR
# error and then remove rpl_fake_commit_order_deadlock_for_timestamp_100
# debug sync points, and then restart slave sql thread.

include/wait_for_slave_sql_error.inc [errno=1105, 1402, 3030]
# Removing debug point 'rpl_fake_commit_order_deadlock_for_timestamp_100' from @@GLOBAL.debug
include/start_slave_sql.inc

# On master, execute XA COMMIT for prepared XA transaction.

[connection master]
XA COMMIT 'xa_test_xid1';

# Cleanup

[connection master]
DROP TABLE t1;
DROP TABLE t2;
SET GLOBAL binlog_transaction_dependency_tracking=@save_binlog_transaction_dependency_tracking;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET GLOBAL replica_parallel_type= @save_replica_parallel_type;
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET GLOBAL replica_parallel_workers= @save_replica_parallel_workers;
SET GLOBAL replica_preserve_commit_order= @save_replica_preserve_commit_order;
include/start_slave.inc
include/rpl_end.inc
