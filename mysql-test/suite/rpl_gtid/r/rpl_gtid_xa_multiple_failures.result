include/only_with_option.inc [GLOBAL.replica_parallel_type = 'LOGICAL_CLOCK']
include/only_with_option.inc [GLOBAL.replica_parallel_workers != 0]
include/only_with_option.inc [GLOBAL.binlog_order_commits = 1]
include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
include/save_sysvars.inc [
"GLOBAL.replica_preserve_commit_order",
"GLOBAL.replica_parallel_workers"
]
SET @@GLOBAL.replica_preserve_commit_order = 0;
SET @@GLOBAL.replica_parallel_workers = 4;
include/start_slave.inc
[connection master]
include/suppress_messages.inc
# Connection 1 suppresses message <Found .* prepared XA transactions>.
# Connection 1 suppresses message <.*Checksum mismatch in datafile.*>.
# Connection 2 suppresses message <Found .* prepared XA transactions>.
# Connection 2 suppresses message <.*Checksum mismatch in datafile.*>.
CREATE TABLE t1 (c1 INT PRIMARY KEY);
XA START 'xid0';
INSERT INTO t1 VALUES (2);
XA END 'xid0';
XA PREPARE 'xid0';
XA COMMIT 'xid0';
include/sync_slave_sql_with_master.inc
include/save_binlog_position.inc
[connection master]
# Adding debug point 'set_commit_parent_100' to @@GLOBAL.debug
XA START 'xid1';
INSERT INTO t1 VALUES (3);
XA END 'xid1';
include/execute_to_conditional_timestamp_sync_point.inc [before_prepare_in_engines]
XA PREPARE 'xid1';
XA START 'xid2';
INSERT INTO t1 VALUES (4);
XA END 'xid2';
include/execute_to_conditional_timestamp_sync_point.inc [after_ha_prepare_low]
XA PREPARE 'xid2';
XA START 'xid3';
INSERT INTO t1 VALUES (5);
XA END 'xid3';
include/execute_to_conditional_timestamp_sync_point.inc [before_set_prepared_in_tc]
XA PREPARE 'xid3';
XA START 'xid4';
INSERT INTO t1 VALUES (6);
XA END 'xid4';
include/execute_to_conditional_timestamp_sync_point.inc [after_writing_to_tc_log]
XA PREPARE 'xid4';
[connection slave]
# Kill the server
# restart:--replica-preserve-commit-order=0
include/rpl_reconnect.inc
include/assert.inc [Found log message(s) for "Rolling back trx with id %, 1 rows to undo"]
include/assert.inc [Found 1 log message(s) for "Successfully rolled back 1, prepared 2 XA transaction"]
include/assert.inc [2 XA transaction(s) in PREPARED state]
include/start_slave.inc
[connection master]
include/sync_slave_sql_with_master.inc
[connection master]
XA ROLLBACK 'xid1';
XA ROLLBACK 'xid2';
XA ROLLBACK 'xid3';
XA ROLLBACK 'xid4';
include/sync_slave_sql_with_master.inc
[connection master]
XA START 'xid5';
INSERT INTO t1 VALUES (7);
XA END 'xid5';
XA PREPARE 'xid5';
XA START 'xid6';
INSERT INTO t1 VALUES (8);
XA END 'xid6';
XA PREPARE 'xid6';
XA START 'xid7';
INSERT INTO t1 VALUES (9);
XA END 'xid7';
XA PREPARE 'xid7';
include/sync_slave_sql_with_master.inc
include/execute_to_conditional_timestamp_sync_point.inc [before_commit_xa_trx]
XA COMMIT 'xid5';
include/execute_to_conditional_timestamp_sync_point.inc [before_invoke_before_commit_hook]
XA COMMIT 'xid6';
include/execute_to_conditional_timestamp_sync_point.inc [before_commit_in_engines]
XA COMMIT 'xid7';
[connection slave]
# Kill the server
# restart:--replica-preserve-commit-order=0
include/rpl_reconnect.inc
include/assert.inc [Found 1 log message(s) for "Successfully committed 1, prepared 2 XA transaction"]
include/assert.inc [2 XA transaction(s) in PREPARED state]
include/start_slave.inc
[connection master]
include/sync_slave_sql_with_master.inc
XA START 'xid8';
INSERT INTO t1 VALUES (10);
XA END 'xid8';
XA PREPARE 'xid8';
XA START 'xid9';
INSERT INTO t1 VALUES (11);
XA END 'xid9';
XA PREPARE 'xid9';
XA START 'xid10';
INSERT INTO t1 VALUES (12);
XA END 'xid10';
XA PREPARE 'xid10';
include/sync_slave_sql_with_master.inc
include/execute_to_conditional_timestamp_sync_point.inc [before_rollback_xa_trx]
XA ROLLBACK 'xid8';
include/execute_to_conditional_timestamp_sync_point.inc [before_invoke_before_commit_hook]
XA ROLLBACK 'xid9';
include/execute_to_conditional_timestamp_sync_point.inc [before_rollback_in_engines]
XA ROLLBACK 'xid10';
[connection slave]
# Kill the server
# restart:--replica-preserve-commit-order=0
include/rpl_reconnect.inc
include/assert.inc [Found 1 log message(s) for "Successfully rolled back 1, prepared 2 XA transaction"]
include/assert.inc [2 XA transaction(s) in PREPARED state]
include/start_slave.inc
[connection master]
UPDATE t1 SET c1 = 200 WHERE c1 = 2;
include/sync_slave_sql_with_master.inc
XA START 'xid11';
INSERT INTO t1 VALUES (13);
XA END 'xid11';
include/execute_to_conditional_timestamp_sync_point.inc [before_prepare_in_engines]
XA COMMIT 'xid11' ONE PHASE;
XA START 'xid12';
INSERT INTO t1 VALUES (14);
XA END 'xid12';
include/execute_to_conditional_timestamp_sync_point.inc [before_invoke_before_commit_hook]
XA COMMIT 'xid12' ONE PHASE;
XA START 'xid13';
INSERT INTO t1 VALUES (15);
XA END 'xid13';
include/execute_to_conditional_timestamp_sync_point.inc [before_commit_in_engines]
XA COMMIT 'xid13' ONE PHASE;
[connection slave]
# Kill the server
# restart:--replica-preserve-commit-order=0
include/rpl_reconnect.inc
include/assert.inc [Found log message(s) for "Rolling back trx with id %, 1 rows to undo"]
include/assert.inc [Found 1 log message(s) for "Successfully committed 1, rolled back 1 XA transaction"]
include/assert.inc [No XA transactions in PREPARED state]
include/restore_sysvars.inc
include/start_slave.inc
[connection master]
# Removing debug point 'set_commit_parent_100' from @@GLOBAL.debug
DROP TABLE t1;
include/rpl_end.inc
