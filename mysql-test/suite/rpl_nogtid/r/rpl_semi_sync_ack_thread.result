include/rpl_init.inc [topology=1->2, 1->3, 1->4]
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection server_1]
CALL mtr.add_suppression("Semi-sync master failed on net_flush().*");
####################################################################
# Test Case: semisync master can be enabled and disabled sucessfully
# without any live slave connection and also test ON, OFF can be set
# more than once without problems.
####################################################################
include/install_semisync_master.inc
SET GLOBAL rpl_semi_sync_master_enabled= ON;
SET GLOBAL rpl_semi_sync_master_enabled= ON;
SET GLOBAL rpl_semi_sync_master_enabled= OFF;
SET GLOBAL rpl_semi_sync_master_enabled= OFF;
####################################################################
# Test Case: Semisync should work even semisync slaves are connected
# before semisync master enabled.
####################################################################
CREATE TABLE t1(c1 INT) ENGINE=InnoDB;
include/rpl_sync.inc
[connection server_2]
include/install_semisync_slave.inc
[connection server_1]
SET GLOBAL rpl_semi_sync_master_enabled= ON;
[connection server_1]
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
INSERT INTO t1 VALUES(3);
TRUNCATE t1;
include/assert.inc [rpl_semi_sync_master_yes_tx increases 4]
include/rpl_sync.inc
####################################################################
# Test Case: Semisync should work when more than one semisync slaves
# connectted.
####################################################################
[connection server_3]
include/install_semisync_slave.inc
[connection server_1]
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
INSERT INTO t1 VALUES(3);
TRUNCATE t1;
include/assert.inc [rpl_semi_sync_master_yes_tx increases 4]
include/rpl_sync.inc
[connection server_4]
include/install_semisync_slave.inc
[connection server_1]
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
INSERT INTO t1 VALUES(3);
TRUNCATE t1;
include/assert.inc [rpl_semi_sync_master_yes_tx increases 4]
include/rpl_sync.inc
###################################################################
# Test Case: Semisync should work after semisync master is disabled
# and enabled again.
###################################################################
[connection server_1]
SET GLOBAL rpl_semi_sync_master_enabled= OFF;
INSERT INTO t1 VALUES(0);
SET GLOBAL rpl_semi_sync_master_enabled= ON;
[connection server_1]
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
INSERT INTO t1 VALUES(3);
TRUNCATE t1;
include/assert.inc [rpl_semi_sync_master_yes_tx increases 4]
include/rpl_sync.inc
###################################################################
# Test Case: Semisync should still work after some semisync slaves
# reconnected
###################################################################
[connection server_2]
include/stop_slave_io.inc
include/start_slave_io.inc
[connection server_1]
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
INSERT INTO t1 VALUES(3);
TRUNCATE t1;
include/assert.inc [rpl_semi_sync_master_yes_tx increases 4]
include/rpl_sync.inc
[connection server_3]
include/stop_slave_io.inc
include/start_slave_io.inc
[connection server_1]
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
INSERT INTO t1 VALUES(3);
TRUNCATE t1;
include/assert.inc [rpl_semi_sync_master_yes_tx increases 4]
include/rpl_sync.inc
###################################################################
# Test Case: Semisync should still work after some semisync slaves
# disconnected
###################################################################
[connection server_2]
include/uninstall_semisync_slave.inc
[connection server_1]
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
INSERT INTO t1 VALUES(3);
TRUNCATE t1;
include/assert.inc [rpl_semi_sync_master_yes_tx increases 4]
include/rpl_sync.inc
[connection server_3]
include/uninstall_semisync_slave.inc
[connection server_1]
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
INSERT INTO t1 VALUES(3);
TRUNCATE t1;
include/assert.inc [rpl_semi_sync_master_yes_tx increases 4]
include/rpl_sync.inc
#####################################################################
# Test Case: PS threads table includes ACK thread's status correctly.
#####################################################################
#
# Status: Waiting for semi-sync ACK from slave
#
[connection server_1]
SELECT name, type, processlist_state FROM performance_schema.threads
WHERE name LIKE "%Ack_receiver";
name	type	processlist_state
thread/semisync/Ack_receiver	BACKGROUND	Waiting for semi-sync ACK from slave
#
# Coverage Test:
# It should print an error to log if select() function returns -1
#
SET @save_debug = @@GLOBAL.debug;
SET GLOBAL debug = 'd,rpl_semisync_simulate_select_error';
INSERT INTO t1 VALUES(1);
SET GLOBAL debug = @save_debug;
#
# Status: Waiting for semisync slave to connect
#
[connection server_4]
include/uninstall_semisync_slave.inc
[connection server_1]
CALL mtr.add_suppression("Timeout waiting for reply of binlog.*");
INSERT INTO t1 VALUES(1);
[connection server_4]
[connection server_1]
SELECT name, type, processlist_state FROM performance_schema.threads
WHERE name LIKE "%Ack_receiver";
name	type	processlist_state
thread/semisync/Ack_receiver	BACKGROUND	Waiting for semi-sync slave connection
#
# Ack thread is stopped, it should be not in PS.threads table.
#
SET GLOBAL rpl_semi_sync_master_enabled= OFF;
include/assert.inc [ack receiver thread is destroied]
############################################
# Test Case: Coverage test on error branches
############################################
SET @save_debug = @@GLOBAL.debug;
CALL mtr.add_suppression("Failed to start semi-sync ACK receiver thread.*");
CALL mtr.add_suppression("Failed to register slave to semi-sync ACK receiver thread.*");
CALL mtr.add_suppression("Failed to stop ack receiver thread on my_thread_join.*");
CALL mtr.add_suppression("Run function 'transmit_start' .* failed");
SET GLOBAL rpl_semi_sync_master_trace_level = 255;
#
# Failure on creating ack receiver thread
#
SET GLOBAL debug = 'd,rpl_semisync_simulate_create_thread_failure';
SET GLOBAL rpl_semi_sync_master_enabled= ON;
include/assert.inc [ should be 0]
include/assert.inc [ack receiver thread is destroied;]
#
# Failure on stopping ack receive thread
#
SET GLOBAL debug= @save_debug;
SET GLOBAL rpl_semi_sync_master_enabled= ON;
SET GLOBAL debug = 'd,rpl_semisync_simulate_thread_join_failure';
SET GLOBAL rpl_semi_sync_master_enabled= OFF;
include/assert.inc [ should be 0]
include/assert.inc [ack receiver thread is destroied]
#
# Failure on registering semisync slave
#
SET GLOBAL debug= 'd,rpl_semisync_simulate_add_slave_failure';
SET GLOBAL rpl_semi_sync_master_enabled= ON;
[connection server_2]
include/stop_slave.inc
CHANGE MASTER TO master_retry_count = 1, master_connect_retry = 5;
SET @save_debug= @@GLOBAL.debug;
SET GLOBAL debug= 'd,dbug.before_get_MASTER_UUID';
include/install_semisync_slave.inc
Warnings:
Note	3084	Replication thread(s) for channel '' are already stopped.
SET GLOBAL debug= @save_debug;
SET debug_sync= 'now SIGNAL signal.get_master_uuid';
include/wait_for_slave_io_to_stop.inc
CHANGE MASTER TO master_retry_count = 10,
master_connect_retry = 1;
include/uninstall_semisync_slave.inc
Warnings:
Note	3084	Replication thread(s) for channel '' are already stopped.
include/start_slave_sql.inc
[connection server_1]
DROP TABLE t1;
include/rpl_sync.inc
SET GLOBAL debug= @save_debug;
include/uninstall_semisync_master.inc
include/rpl_end.inc
