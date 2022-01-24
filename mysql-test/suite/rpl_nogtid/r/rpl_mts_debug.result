include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE DATABASE db_1;
CREATE DATABASE db_2;
CREATE TABLE db_1.test_1 (id INTEGER) engine=Innodb;
CREATE TABLE db_2.test_1 (id INTEGER) engine=Innodb;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
set @save.replica_parallel_workers= @@global.replica_parallel_workers;
set @save.debug= @@global.debug;
SET @@global.replica_parallel_workers= 2;
ALTER TABLE mysql.slave_relay_log_info ENGINE= Innodb;
ALTER TABLE mysql.slave_worker_info ENGINE= Innodb;
include/start_slave.inc
INSERT INTO db_1.test_1 VALUES(0);
include/sync_slave_sql_with_master.inc
SET GLOBAL debug="+d,mta_debug_concurrent_access";
BEGIN;
INSERT INTO db_1.test_1 VALUES(1);
BEGIN;
INSERT INTO db_2.test_1 VALUES(1);
COMMIT;
COMMIT;
DROP DATABASE db_1;
DROP DATABASE db_2;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET @@global.replica_parallel_workers= @save.replica_parallel_workers;
SET @@global.debug= @save.debug;
include/start_slave.inc
call mtr.add_suppression('Error reading slave worker configuration');
call mtr.add_suppression('Failed during slave workers initialization');
call mtr.add_suppression('Failed during slave worker thread creation for channel ''');
include/stop_slave.inc
set @save.replica_checkpoint_group= @@global.replica_checkpoint_group;
set @save.replica_parallel_workers= @@global.replica_parallel_workers;
set @save.debug= @@global.debug;
SET @@global.debug= "+d,mta_worker_thread_fails";
set @@global.replica_parallel_workers= 2;
start slave sql_thread;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_error.inc [errno=13117]
set @@global.replica_checkpoint_group= @save.replica_checkpoint_group;
set @@global.replica_parallel_workers= @save.replica_parallel_workers;
set @@global.debug= @save.debug;
include/assert.inc [Verified that their is no orphaned SQL worker thread]
include/start_slave.inc
include/rpl_reset.inc
include/stop_slave.inc
call mtr.add_suppression("option 'replica_checkpoint_group': unsigned value 524281 adjusted to 524280");
call mtr.add_suppression("Failed during slave worker thread create");
call mtr.add_suppression("Slave SQL for channel '': Failed during slave workers initialization, Error_code: MY-13117");
call mtr.add_suppression("Mismatch between the number of bytes configured to store checkpoint information and the previously stored information");
set @save.replica_checkpoint_group= @@global.replica_checkpoint_group;
set @save.replica_parallel_workers= @@global.replica_parallel_workers;
SET @@global.replica_parallel_workers= 2;
SET @@global.replica_checkpoint_group=524281;
Warnings:
Warning	1292	Truncated incorrect replica_checkpoint_group value: '524281'
include/start_slave.inc
include/assert.inc [Assert that replica_checkpoint_group was truncated to a valid value.]
include/check_slave_is_running.inc
include/rpl_reset.inc
include/stop_slave.inc
SET @@global.debug= "+d,inject_init_worker_init_info_fault";
START SLAVE SQL_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_error.inc [errno=13117]
SET @@global.debug= @save.debug;
set @@global.replica_checkpoint_group= @save.replica_checkpoint_group;
set @@global.replica_parallel_workers= @save.replica_parallel_workers;
call mtr.add_suppression("Could not delete from Slave Workers info repository.");
include/stop_slave.inc
Warnings:
Note	3084	Replication thread(s) for channel '' are already stopped.
set @save.replica_parallel_workers= @@global.replica_parallel_workers;
set @@global.replica_parallel_workers= 2;
include/start_slave.inc
include/stop_slave.inc
set @@global.debug= "+d,mta_debug_reset_workers_fails";
CHANGE REPLICATION SOURCE to RELAY_LOG_FILE=FILE,  RELAY_LOG_POS= POS;
ERROR HY000: Cannot clean up worker info tables. Additional error messages can be found in the MySQL error log.
set @@global.debug= "-d,mta_debug_reset_workers_fails";
set @@global.debug= "+d,mta_debug_recovery_reset_fails";
start slave sql_thread;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_error.inc [errno=13117]
set @@global.debug= @save.debug;
set @@global.replica_parallel_workers= 2;
include/start_slave.inc
include/stop_slave.inc
call mtr.add_suppression("Error creating relay log info: Failed to initialize the worker info structure.");
set @@global.debug= "+d,mta_replica_worker_init_at_gaps_fails";
start slave sql_thread;
ERROR HY000: Could not initialize master info structure; more error messages can be found in the MySQL error log
set @@global.debug= "-d,mta_replica_worker_init_at_gaps_fails";
set @@global.replica_parallel_workers= @save.replica_parallel_workers;
include/start_slave.inc
include/rpl_reset.inc
include/rpl_end.inc
