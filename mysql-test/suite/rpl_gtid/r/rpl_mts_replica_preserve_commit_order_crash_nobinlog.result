include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]

# 1. We start replica (server2) with the binary log turned off. Replica
#    also has replica_preserve_commit_order enabled and running with 4
#    applier workers.

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
SET GLOBAL replica_parallel_workers= 4;
SET GLOBAL replica_preserve_commit_order= ON;
include/start_slave.inc

# 2. On master binlog_transaction_dependency_tracking is set to COMMIT_ORDER.

[connection master]
SET @save_binlog_transaction_dependency_tracking= @@global.binlog_transaction_dependency_tracking;
SET @@global.binlog_transaction_dependency_tracking = COMMIT_ORDER;

# 3. On master add some data and verify that data is successfully
#    replicated to the slave. Also save a copy of slave redo log files to
#    bring the slave server in same state after restart later in step 11.

[connection master]
CREATE TABLE t1 (c1 INT PRIMARY KEY) ENGINE = InnoDB;
CREATE TABLE t2 (c1 INT PRIMARY KEY) ENGINE = InnoDB;
INSERT INTO t2 VALUES (1);
include/sync_slave_sql_with_master.inc
SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
SET GLOBAL innodb_dict_stats_disabled_debug = 1;
SET GLOBAL innodb_master_thread_disabled_debug = 1;
include/assert.inc [Verify t2 contains only one row]

# 4. On server2 enable commit_order_manager_before_wait debug sync point
#    which will be activated for threads having @@session.timestamp 100,
#    101 and 102.

[connection slave]
# Adding debug point 'syncpoint_commit_order_manager_before_wait_100' to @@GLOBAL.debug
# Adding debug point 'syncpoint_commit_order_manager_before_wait_101' to @@GLOBAL.debug
# Adding debug point 'syncpoint_commit_order_manager_before_wait_102' to @@GLOBAL.debug

# 5. On master generate transactions (T1, T2 and T3) with timestamp
#    100, 101 and 102 which can be applied in parallel,
#    by setting same commit parents for all the inserts.

[connection master]
# Adding debug point 'set_commit_parent_100' to @@GLOBAL.debug
[connection server_1]
SET TIMESTAMP = 100;
BEGIN;
INSERT INTO t1 VALUES (1);
COMMIT;
[connection server1_1]
SET TIMESTAMP = 101;
BEGIN;
INSERT INTO t2 VALUES (2);
COMMIT;
[connection server1_1_1]
SET TIMESTAMP = 102;
BEGIN;
INSERT INTO t1 VALUES (2);
COMMIT;
# Removing debug point 'set_commit_parent_100' from @@GLOBAL.debug

# 6. The slave threads executing inserts (added in step 5 on master)
#    wait on commit_order_manager_before_wait debug sync point.
#    Wait for all the threads to reach commit_order_manager_before_wait
#    debug sync point, so that they use same flush stage queue.

[connection slave]
SET DEBUG_SYNC= 'now WAIT_FOR reached_commit_order_manager_before_wait_100';
SET DEBUG_SYNC= 'now WAIT_FOR reached_commit_order_manager_before_wait_101';
SET DEBUG_SYNC= 'now WAIT_FOR reached_commit_order_manager_before_wait_102';

# 7. Enable after_commit_order_thread_becomes_leader and
#    before_follower_wait debug sync point which would make
#    leader (thread executing T1) and follower threads (thread executing
#    T2, T3) wait on their respective debug sync point.
#    Also release all the threads waiting on
#    commit_order_manager_before_wait debug sync point.

# Adding debug point 'syncpoint_after_commit_order_thread_becomes_leader_100' to @@GLOBAL.debug
# Adding debug point 'syncpoint_before_follower_wait_101' to @@GLOBAL.debug
# Adding debug point 'syncpoint_before_follower_wait_102' to @@GLOBAL.debug
SET DEBUG_SYNC= 'now SIGNAL continue_commit_order_manager_before_wait_100';
SET DEBUG_SYNC= 'now SIGNAL continue_commit_order_manager_before_wait_101';
SET DEBUG_SYNC= 'now SIGNAL continue_commit_order_manager_before_wait_102';

# 8. Now wait for leader (T1) and followers (T2, T3) to reach
#    after_commit_order_thread_becomes_leader and
#    before_follower_wait debug sync point. After leaders
#    and follower threads reach there respective debug sync point signal
#    follower threads to awake and continue.

SET DEBUG_SYNC= "now WAIT_FOR reached_after_commit_order_thread_becomes_leader_100";
SET DEBUG_SYNC= 'now WAIT_FOR reached_before_follower_wait_101';
SET DEBUG_SYNC= 'now WAIT_FOR reached_before_follower_wait_102';
SET DEBUG_SYNC= 'now SIGNAL continue_before_follower_wait_101';
SET DEBUG_SYNC= 'now SIGNAL continue_before_follower_wait_102';

# 9. Now enable commit_order_leader_before_ha_flush_logs debug sync point
#    which will make leader wait before it flush transactions of all
#    waiting threads to the storage engine. It also unblocks leader from
#    after_commit_order_thread_becomes_leader and waits for it to reach
#    commit_order_leader_before_ha_flush_logs debug sync point.

# Adding debug point 'syncpoint_commit_order_leader_before_ha_flush_logs_100' to @@GLOBAL.debug
SET DEBUG_SYNC= "now SIGNAL continue_after_commit_order_thread_becomes_leader_100";
SET DEBUG_SYNC= "now WAIT_FOR reached_commit_order_leader_before_ha_flush_logs_100";

# 10. The server is crashed before leader flush transactions of
#     waiting threads to the storage engine.

SET SESSION debug = '+d,crash_now_safe';
ERROR HY000: Lost connection to MySQL server during query

# 11. When data inserted in step 5 is successfully received by slave,
#     server aborts before it flushes to disk. We wait for slave to
#     shutdown and after it is down we replace redo log files with files
#     saved in step 3 and then restart slave.

[connection slave]
include/rpl_reconnect.inc
[connection slave]

# 12. Verify data inserted in step 5 is not there on the slave as slave
#     was aborted before data was flushed.

include/assert.inc [Verify t1 does not contain any data]
include/assert.inc [Verify t2 contain only one row]

# 13. Start replication on server2 and verify that missing data is now
#     successfully replicated to the slave.

RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
SET GLOBAL replica_parallel_type = 'LOGICAL_CLOCK';
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET GLOBAL replica_parallel_workers= 4;
SET GLOBAL replica_preserve_commit_order= ON;
include/start_slave.inc
[connection master]
include/sync_slave_sql_with_master.inc
[connection slave]
include/assert.inc [Verify t1 contains all the data]
include/assert.inc [Verify t2 contains all the data]

# 14. Cleanup

[connection master]
DROP TABLE t1;
DROP TABLE t2;
SET GLOBAL binlog_transaction_dependency_tracking=@save_binlog_transaction_dependency_tracking;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
