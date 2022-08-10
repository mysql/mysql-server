include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
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
SET GLOBAL replica_parallel_workers= 8;
SET GLOBAL replica_preserve_commit_order= ON;
include/start_slave.inc
[connection master]
SET @save_binlog_transaction_dependency_tracking= @@global.binlog_transaction_dependency_tracking;
SET @@global.binlog_transaction_dependency_tracking = COMMIT_ORDER;
[connection master]
CREATE TABLE t1 (c1 INT PRIMARY KEY) ENGINE = InnoDB; CREATE TABLE t2 (c1 INT PRIMARY KEY) ENGINE = MYISAM;;
include/sync_slave_sql_with_master.inc

# Block slave sql applier threads

[connection slave]
BEGIN; INSERT INTO t1 VALUES (1);;

# Generate the transactions which can be applied in parallel on slave

[connection master]
# Adding debug point 'set_commit_parent_100' to @@GLOBAL.debug
BEGIN; INSERT INTO t1 VALUES (1); COMMIT; BEGIN; INSERT INTO t1 VALUES (2); COMMIT; REPAIR TABLE t2 QUICK; BEGIN; INSERT INTO t1 VALUES (4); COMMIT;;
Table	Op	Msg_type	Msg_text
test.t2	repair	status	OK
[connection server_1]
BEGIN; INSERT INTO t1 VALUES (5); COMMIT; BEGIN; INSERT INTO t1 VALUES (6); COMMIT; BEGIN; INSERT INTO t1 VALUES (7); COMMIT;;
# Removing debug point 'set_commit_parent_100' from @@GLOBAL.debug

# Verify the transactions are ordered correctly on slave

[connection server_2]
include/assert.inc [Verify table t1 is empty]

# Rollback the first insert so that slave applier threads can
# unblock and proceed. Verify the transactions are applied.

[connection slave]
ROLLBACK;;
DROP TABLE t1; DROP TABLE t2;;
include/sync_slave_sql_with_master.inc

# Cleanup

[connection master]
SET GLOBAL binlog_transaction_dependency_tracking=@save_binlog_transaction_dependency_tracking;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET GLOBAL replica_parallel_type=@save_replica_parallel_type;
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET GLOBAL replica_parallel_workers=@save_replica_parallel_workers;
SET GLOBAL replica_preserve_commit_order=@save_replica_preserve_commit_order;
include/start_slave.inc
include/rpl_end.inc
