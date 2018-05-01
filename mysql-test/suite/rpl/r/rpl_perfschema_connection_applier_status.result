include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
# Adding debug point 'rpl_ps_tables' to @@GLOBAL.debug
include/stop_slave_io.inc
[connection master]
CREATE TABLE t (a int);
[connection slave]
# Adding debug point 'rpl_ps_tables_queue' to @@GLOBAL.debug
include/start_slave_io.inc
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_queue_before';
# Removing debug point 'rpl_ps_tables_queue' from @@GLOBAL.debug
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_queue_finish';
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_queue_after_finish';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_queue_continue';
include/assert.inc [Assert that the currently queueing gtid is equal to current_gtid]
include/assert.inc [Assert that the last queued gtid is equal to last_gtid]
include/assert.inc [Assert that currently queueing gtid was cleared]
include/assert.inc [Assert that currently queueing OCT was cleared]
include/assert.inc [Assert that currently queueing ICT was cleared]
include/assert.inc [Assert that currently queueing start was cleared]
include/assert.inc [Assert that the last queued gtid is equal to current_gtid]
include/assert.inc [Assert that currently queueing OCT exists]
include/assert.inc [Assert that currently queueing ICT exists]
include/assert.inc [Assert that currently queueing start_time exists]
include/assert.inc [Assert that the OCT of the last queued and currently queueing is the same]
include/assert.inc [Assert that the ICT of the last queued and currently queueing is the same]
include/assert.inc [Assert that the start time of the last queued and currently queueing is the same]
include/assert.inc [Assert that the end time of the last queued is greater than the start time]
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_apply_before';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_apply_finish';
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_apply_after_finish';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_apply_continue';
include/assert.inc [Assert that the currently applying gtid is equal to current_gtid]
include/assert.inc [Assert that the last applied gtid is equal to last_gtid]
include/assert.inc [Assert that currently applying gtid was cleared]
include/assert.inc [Assert that currently applying OCT was cleared]
include/assert.inc [Assert that currently applying ICT was cleared]
include/assert.inc [Assert that currently applying start was cleared]
include/assert.inc [Assert that the last applied gtid is equal to current_gtid]
include/assert.inc [Assert that currently applying OCT exists]
include/assert.inc [Assert that currently applying ICT exists]
include/assert.inc [Assert that currently applying start_time exists]
include/assert.inc [Assert that the OCT of the last applied and currently applying is the same]
include/assert.inc [Assert that the ICT of the last applied and currently applying is the same]
include/assert.inc [Assert that the start time of the last applied and currently applying is the same]
include/assert.inc [Assert that the end time of the last applied is greater than the start time]
include/stop_slave_io.inc
[connection master]
INSERT INTO t VALUES(1);
[connection slave]
# Adding debug point 'rpl_ps_tables_queue' to @@GLOBAL.debug
include/start_slave_io.inc
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_queue_before';
# Removing debug point 'rpl_ps_tables_queue' from @@GLOBAL.debug
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_queue_finish';
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_queue_after_finish';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_queue_continue';
include/assert.inc [Assert that the currently queueing gtid is equal to current_gtid]
include/assert.inc [Assert that the last queued gtid is equal to last_gtid]
include/assert.inc [Assert that currently queueing gtid was cleared]
include/assert.inc [Assert that currently queueing OCT was cleared]
include/assert.inc [Assert that currently queueing ICT was cleared]
include/assert.inc [Assert that currently queueing start was cleared]
include/assert.inc [Assert that the last queued gtid is equal to current_gtid]
include/assert.inc [Assert that currently queueing OCT exists]
include/assert.inc [Assert that currently queueing ICT exists]
include/assert.inc [Assert that currently queueing start_time exists]
include/assert.inc [Assert that the OCT of the last queued and currently queueing is the same]
include/assert.inc [Assert that the ICT of the last queued and currently queueing is the same]
include/assert.inc [Assert that the start time of the last queued and currently queueing is the same]
include/assert.inc [Assert that the end time of the last queued is greater than the start time]
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_apply_before';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_apply_finish';
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_apply_after_finish';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_apply_continue';
include/assert.inc [Assert that the currently applying gtid is equal to current_gtid]
include/assert.inc [Assert that the last applied gtid is equal to last_gtid]
include/assert.inc [Assert that currently applying gtid was cleared]
include/assert.inc [Assert that currently applying OCT was cleared]
include/assert.inc [Assert that currently applying ICT was cleared]
include/assert.inc [Assert that currently applying start was cleared]
include/assert.inc [Assert that the last applied gtid is equal to current_gtid]
include/assert.inc [Assert that currently applying OCT exists]
include/assert.inc [Assert that currently applying ICT exists]
include/assert.inc [Assert that currently applying start_time exists]
include/assert.inc [Assert that the OCT of the last applied and currently applying is the same]
include/assert.inc [Assert that the ICT of the last applied and currently applying is the same]
include/assert.inc [Assert that the start time of the last applied and currently applying is the same]
include/assert.inc [Assert that the end time of the last applied is greater than the start time]
include/stop_slave_io.inc
[connection master]
UPDATE t SET a=2;
[connection slave]
# Adding debug point 'rpl_ps_tables_queue' to @@GLOBAL.debug
include/start_slave_io.inc
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_queue_before';
# Removing debug point 'rpl_ps_tables_queue' from @@GLOBAL.debug
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_queue_finish';
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_queue_after_finish';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_queue_continue';
include/assert.inc [Assert that the currently queueing gtid is equal to current_gtid]
include/assert.inc [Assert that the last queued gtid is equal to last_gtid]
include/assert.inc [Assert that currently queueing gtid was cleared]
include/assert.inc [Assert that currently queueing OCT was cleared]
include/assert.inc [Assert that currently queueing ICT was cleared]
include/assert.inc [Assert that currently queueing start was cleared]
include/assert.inc [Assert that the last queued gtid is equal to current_gtid]
include/assert.inc [Assert that currently queueing OCT exists]
include/assert.inc [Assert that currently queueing ICT exists]
include/assert.inc [Assert that currently queueing start_time exists]
include/assert.inc [Assert that the OCT of the last queued and currently queueing is the same]
include/assert.inc [Assert that the ICT of the last queued and currently queueing is the same]
include/assert.inc [Assert that the start time of the last queued and currently queueing is the same]
include/assert.inc [Assert that the end time of the last queued is greater than the start time]
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_apply_before';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_apply_finish';
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_apply_after_finish';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_apply_continue';
include/assert.inc [Assert that the currently applying gtid is equal to current_gtid]
include/assert.inc [Assert that the last applied gtid is equal to last_gtid]
include/assert.inc [Assert that currently applying gtid was cleared]
include/assert.inc [Assert that currently applying OCT was cleared]
include/assert.inc [Assert that currently applying ICT was cleared]
include/assert.inc [Assert that currently applying start was cleared]
include/assert.inc [Assert that the last applied gtid is equal to current_gtid]
include/assert.inc [Assert that currently applying OCT exists]
include/assert.inc [Assert that currently applying ICT exists]
include/assert.inc [Assert that currently applying start_time exists]
include/assert.inc [Assert that the OCT of the last applied and currently applying is the same]
include/assert.inc [Assert that the ICT of the last applied and currently applying is the same]
include/assert.inc [Assert that the start time of the last applied and currently applying is the same]
include/assert.inc [Assert that the end time of the last applied is greater than the start time]
include/stop_slave_io.inc
[connection master]
DELETE FROM t WHERE a=2;
[connection slave]
# Adding debug point 'rpl_ps_tables_queue' to @@GLOBAL.debug
include/start_slave_io.inc
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_queue_before';
# Removing debug point 'rpl_ps_tables_queue' from @@GLOBAL.debug
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_queue_finish';
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_queue_after_finish';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_queue_continue';
include/assert.inc [Assert that the currently queueing gtid is equal to current_gtid]
include/assert.inc [Assert that the last queued gtid is equal to last_gtid]
include/assert.inc [Assert that currently queueing gtid was cleared]
include/assert.inc [Assert that currently queueing OCT was cleared]
include/assert.inc [Assert that currently queueing ICT was cleared]
include/assert.inc [Assert that currently queueing start was cleared]
include/assert.inc [Assert that the last queued gtid is equal to current_gtid]
include/assert.inc [Assert that currently queueing OCT exists]
include/assert.inc [Assert that currently queueing ICT exists]
include/assert.inc [Assert that currently queueing start_time exists]
include/assert.inc [Assert that the OCT of the last queued and currently queueing is the same]
include/assert.inc [Assert that the ICT of the last queued and currently queueing is the same]
include/assert.inc [Assert that the start time of the last queued and currently queueing is the same]
include/assert.inc [Assert that the end time of the last queued is greater than the start time]
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_apply_before';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_apply_finish';
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_apply_after_finish';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_apply_continue';
include/assert.inc [Assert that the currently applying gtid is equal to current_gtid]
include/assert.inc [Assert that the last applied gtid is equal to last_gtid]
include/assert.inc [Assert that currently applying gtid was cleared]
include/assert.inc [Assert that currently applying OCT was cleared]
include/assert.inc [Assert that currently applying ICT was cleared]
include/assert.inc [Assert that currently applying start was cleared]
include/assert.inc [Assert that the last applied gtid is equal to current_gtid]
include/assert.inc [Assert that currently applying OCT exists]
include/assert.inc [Assert that currently applying ICT exists]
include/assert.inc [Assert that currently applying start_time exists]
include/assert.inc [Assert that the OCT of the last applied and currently applying is the same]
include/assert.inc [Assert that the ICT of the last applied and currently applying is the same]
include/assert.inc [Assert that the start time of the last applied and currently applying is the same]
include/assert.inc [Assert that the end time of the last applied is greater than the start time]
include/stop_slave_io.inc
[connection master]
DROP TABLE t;
[connection slave]
# Adding debug point 'rpl_ps_tables_queue' to @@GLOBAL.debug
include/start_slave_io.inc
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_queue_before';
# Removing debug point 'rpl_ps_tables_queue' from @@GLOBAL.debug
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_queue_finish';
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_queue_after_finish';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_queue_continue';
include/assert.inc [Assert that the currently queueing gtid is equal to current_gtid]
include/assert.inc [Assert that the last queued gtid is equal to last_gtid]
include/assert.inc [Assert that currently queueing gtid was cleared]
include/assert.inc [Assert that currently queueing OCT was cleared]
include/assert.inc [Assert that currently queueing ICT was cleared]
include/assert.inc [Assert that currently queueing start was cleared]
include/assert.inc [Assert that the last queued gtid is equal to current_gtid]
include/assert.inc [Assert that currently queueing OCT exists]
include/assert.inc [Assert that currently queueing ICT exists]
include/assert.inc [Assert that currently queueing start_time exists]
include/assert.inc [Assert that the OCT of the last queued and currently queueing is the same]
include/assert.inc [Assert that the ICT of the last queued and currently queueing is the same]
include/assert.inc [Assert that the start time of the last queued and currently queueing is the same]
include/assert.inc [Assert that the end time of the last queued is greater than the start time]
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_apply_before';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_apply_finish';
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_apply_after_finish';
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_apply_continue';
include/assert.inc [Assert that the currently applying gtid is equal to current_gtid]
include/assert.inc [Assert that the last applied gtid is equal to last_gtid]
include/assert.inc [Assert that currently applying gtid was cleared]
include/assert.inc [Assert that currently applying OCT was cleared]
include/assert.inc [Assert that currently applying ICT was cleared]
include/assert.inc [Assert that currently applying start was cleared]
include/assert.inc [Assert that the last applied gtid is equal to current_gtid]
include/assert.inc [Assert that currently applying OCT exists]
include/assert.inc [Assert that currently applying ICT exists]
include/assert.inc [Assert that currently applying start_time exists]
include/assert.inc [Assert that the OCT of the last applied and currently applying is the same]
include/assert.inc [Assert that the ICT of the last applied and currently applying is the same]
include/assert.inc [Assert that the start time of the last applied and currently applying is the same]
include/assert.inc [Assert that the end time of the last applied is greater than the start time]
# Removing debug point 'rpl_ps_tables' from @@GLOBAL.debug
include/stop_slave.inc
include/assert.inc [Assert that the gtid is kept while the slave is stopped]
include/start_slave.inc
include/assert.inc [Assert that the gtid is kept after the slave was stopped and started]
[connection master]
SET SESSION GTID_NEXT= AUTOMATIC;
include/sync_slave_sql_with_master.inc
[connection master]
SET SQL_LOG_BIN= 0;
CREATE TABLE t1(i INT);
SET SQL_LOG_BIN= 1;
[connection slave]
call mtr.add_suppression("Slave SQL for channel '': .*Error .Unknown table .test.t1.. on query.* Error_code: MY-001051");
call mtr.add_suppression("Slave: Unknown table .test.t1. Error_code: MY-001051");
call mtr.add_suppression("Slave SQL for channel '': .*Error executing row event: 'Table 'test.t1' doesn't exist'");
call mtr.add_suppression("Slave SQL for channel '': ... The slave coordinator and worker threads are stopped, possibly leaving data in inconsistent state.*");
[connection master]
DROP TABLE t1;
[connection slave]
include/wait_for_slave_sql_error.inc [errno=1051]
include/assert.inc [LAST_APPLIED_TRANSACTION did not change after the failed DDL]
include/assert.inc [APPLYING_TRANSACTION is equal to the failed transaction GTID]
include/stop_slave.inc
[connection master]
include/rpl_reset.inc
SET SQL_LOG_BIN= 0;
CREATE TABLE t1(i INT);
SET SQL_LOG_BIN= 1;
INSERT INTO t1 VALUES (1);
[connection slave]
include/wait_for_slave_sql_error.inc [errno=1146]
include/assert.inc [LAST_APPLIED_TRANSACTION did not change after the failed DML]
include/assert.inc [APPLYING_TRANSACTION is equal to the failed transaction GTID]
[connection master]
SET SQL_LOG_BIN= 0;
DROP TABLE t1;
SET SQL_LOG_BIN= 1;
include/rpl_reset.inc
[connection master]
[connection slave]
include/stop_slave.inc
SET GLOBAL slave_parallel_workers= 4;
include/start_slave.inc
include/assert.inc [processing_gtid is cleared when a former STS slave restarts with MTS enabled]
include/assert.inc [last_processed_gtid is cleared when a former STS slave restarts with MTS enabled]
include/stop_slave.inc
SET GLOBAL slave_parallel_workers= 0;
include/start_slave.inc
include/rpl_end.inc
