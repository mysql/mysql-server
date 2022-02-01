include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]

# Setup

[connection slave]
CALL mtr.add_suppression("You need to use --log-bin to make --binlog-format work");
SET @save_slave_parallel_workers= @@global.slave_parallel_workers;
SET @save_slave_parallel_type= @@global.slave_parallel_type;
SET @save_slave_preserve_commit_order= @@global.slave_preserve_commit_order;
SET GLOBAL slave_parallel_type = 'LOGICAL_CLOCK';
SET GLOBAL slave_parallel_workers= 2;
SET GLOBAL slave_preserve_commit_order= ON;
include/start_slave.inc
[connection master]
SET @save_binlog_transaction_dependency_tracking= @@global.binlog_transaction_dependency_tracking;
SET @@global.binlog_transaction_dependency_tracking = COMMIT_ORDER;
[connection master]
CREATE TABLE t1 (c1 INT PRIMARY KEY) ENGINE = INNODB; CREATE TABLE t2 (c1 INT PRIMARY KEY) ENGINE = INNODB;;
include/sync_slave_sql_with_master.inc

# Block slave sql applier threads

[connection slave]
LOCK TABLE t2 READ;;

# Generate the transactions which can be applied in parallel on slave

[connection master]
# Adding debug point 'set_commit_parent_100' to @@GLOBAL.debug
ALTER TABLE t2 ADD COLUMN c2 INT; INSERT INTO t1 VALUES (1);;
[connection server_1]
BEGIN; INSERT INTO t2 VALUES (1, 1); INSERT INTO t1 VALUES (2); COMMIT;;
# Removing debug point 'set_commit_parent_100' from @@GLOBAL.debug

# Verify the transactions are ordered correctly on slave

[connection server_2]
include/assert.inc [Verify table t1 is empty]

# Rollback the first insert so that slave applier threads can
# unblock and proceed. Verify the transactions are applied.

[connection slave]
UNLOCK TABLES;;
[connection master]
include/sync_slave_sql_with_master.inc
include/assert.inc [Verify table t1 has data]
include/assert.inc [Verify table t2 has data]

# Cleanup

[connection master]
DROP TABLE t1;
DROP TABLE t2;
SET GLOBAL binlog_transaction_dependency_tracking=@save_binlog_transaction_dependency_tracking;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET GLOBAL slave_parallel_type= @save_slave_parallel_type;
SET GLOBAL slave_parallel_workers= @save_slave_parallel_workers;
SET GLOBAL slave_preserve_commit_order= @save_slave_preserve_commit_order;
include/start_slave.inc
include/rpl_end.inc
