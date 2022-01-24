include/only_with_option.inc [GLOBAL.replica_parallel_workers = 4]
include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
SET @saved_innodb_lock_wait_timeout = @@GLOBAL.innodb_lock_wait_timeout;
SET GLOBAL innodb_lock_wait_timeout = 1000;
include/restart_slave.inc
[connection master]
CREATE TABLE t1(c1 INT PRIMARY KEY);
CREATE TABLE t2(c1 INT PRIMARY KEY);
include/sync_slave_sql_with_master.inc
BEGIN;
INSERT INTO t1 VALUES(1);
[connection slave1]
BEGIN;
INSERT INTO t2 VALUES(1);
[connection master]
SET debug = '+d,set_commit_parent_100';
INSERT INTO t1 VALUES(1);
INSERT INTO t2 VALUES(1);
SET debug = '-d,set_commit_parent_100';
INSERT INTO t1 VALUES(2);
SET debug = '+d,logical_clock_step_2';
INSERT INTO t1 VALUES(3);
SET debug = '-d,logical_clock_step_2';
include/save_master_pos.inc
[connection slave]
SET GLOBAL debug = '+d,replica_worker_ends_group_before_signal_lwm';
ROLLBACK;
SET GLOBAL debug = '-d,replica_worker_ends_group_before_signal_lwm';
SET GLOBAL debug = '+d,wait_for_workers_to_finish_after_wait';
[connection slave1]
ROLLBACK;
SET debug_sync='now SIGNAL worker_continue';
SET debug_sync='now SIGNAL coordinator_continue';
include/sync_slave_sql.inc
SET GLOBAL debug = '-d,wait_for_workers_to_finish_after_wait';
[connection slave]
SET GLOBAL innodb_lock_wait_timeout = @saved_innodb_lock_wait_timeout;
[connection master]
DROP TABLE t1, t2;
include/rpl_end.inc
