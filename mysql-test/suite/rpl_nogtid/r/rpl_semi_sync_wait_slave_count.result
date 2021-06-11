include/rpl_init.inc [topology=1->2, 1->3, 1->4, 1->5]
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
CREATE TABLE t1(c1 INT);
include/rpl_sync.inc
[connection server_1]
include/install_semisync_source.inc
SET GLOBAL rpl_semi_sync_source_trace_level= 255;
####################################################################
# Test Case: Wait ack from 1 slave, but no semisync slave is enabled
####################################################################
SET GLOBAL rpl_semi_sync_source_timeout=3000;
INSERT INTO t1 VALUES(1);
include/assert.inc [rpl_semi_sync_source_yes_tx should be 0 + 0]
include/assert.inc [rpl_semi_sync_source_no_tx should be 0 + 1]
###################################################################
# Test Case: Wait ack from 1 slave, and 1 semisync slave is enabled
###################################################################
[connection server_2]
include/install_semisync_replica.inc
[connection server_1]
SET GLOBAL rpl_semi_sync_source_timeout=100000;
INSERT INTO t1 VALUES(3);
INSERT INTO t1 VALUES(4);
include/assert.inc [rpl_semi_sync_source_yes_tx should be 0 + 2]
include/assert.inc [rpl_semi_sync_source_no_tx should be 1 + 0]
#########################################################################
# Test Case: Wait ack from 2 slaves, but only 1 semisync slave is enabled
#########################################################################
SET GLOBAL rpl_semi_sync_source_wait_for_replica_count = 2;
SET GLOBAL rpl_semi_sync_source_timeout=3000;
INSERT INTO t1 VALUES(10);
include/assert.inc [rpl_semi_sync_source_yes_tx should be 2 + 0]
include/assert.inc [rpl_semi_sync_source_no_tx should be 1 + 1]
#####################################################################
# Test Case: Wait ack from 2 slaves, and 2 semisync slave are enabled
#####################################################################
[connection server_3]
include/install_semisync_replica.inc
[connection server_1]
SET GLOBAL rpl_semi_sync_source_timeout = 100000;
INSERT INTO t1 VALUES(102);
INSERT INTO t1 VALUES(103);
include/assert.inc [rpl_semi_sync_source_yes_tx should be 2 + 2]
include/assert.inc [rpl_semi_sync_source_no_tx should be 2 + 0]
##########################################################################
# Test Case: Wait ack from 3 slaves, but only 2 semisync slave are enabled
##########################################################################
SET GLOBAL rpl_semi_sync_source_wait_for_replica_count = 3;
SET GLOBAL rpl_semi_sync_source_timeout = 3000;
INSERT INTO t1 VALUES(100);
INSERT INTO t1 VALUES(101);
include/assert.inc [rpl_semi_sync_source_yes_tx should be 4 + 0]
include/assert.inc [rpl_semi_sync_source_no_tx should be 2 + 2]
######################################################################
# Test Case: Wait ack from 3 slaves, and 3 semisync slaves are enabled
######################################################################
[connection server_4]
include/install_semisync_replica.inc
[connection server_1]
SET GLOBAL rpl_semi_sync_source_timeout = 100000;
INSERT INTO t1 VALUES(102);
INSERT INTO t1 VALUES(103);
include/assert.inc [rpl_semi_sync_source_yes_tx should be 4 + 2]
include/assert.inc [rpl_semi_sync_source_no_tx should be 4 + 0]
######################################################################
# Test Case: Verify only the transaction before replied ack position
#            can stop waiting and go ahead.
######################################################################
[connection server_4]
SET @original_debug = @@GLOBAL.debug;
SET GLOBAL debug = '+d,rpl_semisync_before_send_ack';
[connection default]
INSERT INTO t1 VALUES(104);
[connection server_2]
[connection server_1]
INSERT INTO t1 VALUES(105);
[connection server_2]
[connection server_3]
[connection server_4]
SET DEBUG_SYNC = 'now SIGNAL continue';
[connection default]
include/assert.inc [rpl_semi_sync_source_yes_tx should be 6 + 1]
include/assert.inc [rpl_semi_sync_source_no_tx should be 4 + 0]
[connection server_4]
SET DEBUG_SYNC = 'now SIGNAL continue';
[connection server_1]
include/assert.inc [rpl_semi_sync_source_yes_tx should be 7 + 1]
include/assert.inc [rpl_semi_sync_source_no_tx should be 4 + 0]
######################################################################
# Test Case: Coverage test, it covers the branch that coming ack is
#            greater than the acked already received.
######################################################################
[connection server_1]
INSERT INTO t1 VALUES(130);;
[connection server_2]
SET @original_debug = @@GLOBAL.debug;
SET GLOBAL debug = '+d,rpl_semisync_before_send_ack';
[connection server_3]
SET @original_debug = @@GLOBAL.debug;
SET GLOBAL debug = '+d,rpl_semisync_before_send_ack';
[connection default]
INSERT INTO t1 VALUES(131);;
[connection server_5]
include/install_semisync_replica.inc
[connection server_1]
include/assert.inc [rpl_semi_sync_source_yes_tx should be 8 + 1]
include/assert.inc [rpl_semi_sync_source_no_tx should be 4 + 0]
[connection server_2]
SET GLOBAL debug = @original_debug;
SET DEBUG_SYNC= 'now SIGNAL continue';
[connection server_3]
SET GLOBAL debug = @original_debug;
SET DEBUG_SYNC= 'now SIGNAL continue';
[connection server_4]
SET GLOBAL debug = @original_debug;
SET DEBUG_SYNC= 'now SIGNAL continue';
[connection default]
include/assert.inc [rpl_semi_sync_source_yes_tx should be 9 + 1]
include/assert.inc [rpl_semi_sync_source_no_tx should be 4 + 0]
######################################################################
# Test Case: Verify it can work well when number of semisync slaves
# is greater than rpl_semi_sync_source_wait_for_replica_count
######################################################################
[connection server_1]
SET GLOBAL rpl_semi_sync_source_wait_for_replica_count = 2;
INSERT INTO t1 VALUES(106);
INSERT INTO t1 VALUES(107);
include/rpl_sync.inc
include/assert.inc [rpl_semi_sync_source_yes_tx should be 10 + 2]
include/assert.inc [rpl_semi_sync_source_no_tx should be 4 + 0]
SET GLOBAL rpl_semi_sync_source_wait_for_replica_count = 4;
INSERT INTO t1 VALUES(108);
INSERT INTO t1 VALUES(109);
include/assert.inc [rpl_semi_sync_source_yes_tx should be 12 + 2]
include/assert.inc [rpl_semi_sync_source_no_tx should be 4 + 0]
######################################################################
# Test Case: It should still keep the old value and works well when
# setting rpl_semi_sync_source_wait_for_replica_count fails
######################################################################
SET @original_debug = @@GLOBAL.debug;
SET GLOBAL debug = '+d,rpl_semisync_simulate_allocate_ack_container_failure';
SET GLOBAL rpl_semi_sync_source_wait_for_replica_count = 2;
include/assert.inc [ should be 4]
INSERT INTO t1 VALUES(110);
INSERT INTO t1 VALUES(111);
include/assert.inc [rpl_semi_sync_source_yes_tx should be 14 + 2]
include/assert.inc [rpl_semi_sync_source_no_tx should be 4 + 0]
SET GLOBAL rpl_semi_sync_source_wait_for_replica_count = 5;
include/assert.inc [ should be 4]
INSERT INTO t1 VALUES(112);
INSERT INTO t1 VALUES(113);
include/assert.inc [rpl_semi_sync_source_yes_tx should be 16 + 2]
include/assert.inc [rpl_semi_sync_source_no_tx should be 4 + 0]
SET GLOBAL debug = @original_debug;
########################################################################
# Test Case: Verify it works well when some semisync slaves shutdown and
#            rpl_semi_sync_source_wait_for_replica_count is decreased.
########################################################################
SET GLOBAL rpl_semi_sync_source_wait_no_replica = 1;
SET GLOBAL rpl_semi_sync_source_timeout = 90000000;
[connection server_5]
include/uninstall_semisync_replica.inc
[connection server_4]
include/uninstall_semisync_replica.inc
[connection default]
INSERT INTO t1 VALUES(201);
[connection server_1]
include/rpl_sync.inc
SET GLOBAL rpl_semi_sync_source_wait_for_replica_count = 2;
[connection default]
include/assert.inc [rpl_semi_sync_source_yes_tx should be 18 + 1]
include/assert.inc [rpl_semi_sync_source_no_tx should be 4 + 0]
[connection server_3]
include/uninstall_semisync_replica.inc
[connection default]
INSERT INTO t1 VALUES(202);
[connection server_1]
include/rpl_sync.inc
SET GLOBAL rpl_semi_sync_source_wait_for_replica_count = 1;
[connection default]
include/assert.inc [rpl_semi_sync_source_yes_tx should be 19 + 1]
include/assert.inc [rpl_semi_sync_source_no_tx should be 4 + 0]
########################################################################
# Test Case: Verify semisync master will turn off immediately after a
#            a semisync slave unregistered and there are no enough
#            semisync slaves
########################################################################
SET GLOBAL rpl_semi_sync_source_wait_no_replica = 0;
[connection server_2]
include/uninstall_semisync_replica.inc
[connection server_1]
########################################################################
# Coverage Test
########################################################################
SET GLOBAL rpl_semi_sync_source_wait_for_replica_count = 4;
SET GLOBAL rpl_semi_sync_source_wait_for_replica_count = 4;
SET GLOBAL rpl_semi_sync_source_enabled= OFF;
SET GLOBAL rpl_semi_sync_source_enabled= OFF;
SET GLOBAL rpl_semi_sync_source_enabled= ON;
include/uninstall_semisync_source.inc
[connection server_1]
DROP TABLE t1;
CALL mtr.add_suppression(".* Timeout waiting for reply of binlog .*");
CALL mtr.add_suppression(".* Failed to allocate memory for ack_array .*");
include/rpl_end.inc
