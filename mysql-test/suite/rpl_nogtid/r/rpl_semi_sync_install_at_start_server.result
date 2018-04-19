include/rpl_init.inc [topology=1->2,1->3]
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
CALL mtr.add_suppression(".*Got an error reading communication packets.*");
#
# Semisync master's status should be OFF if
# rpl_semi_sync_master_wait_no_slave is set to 0 at starting server.
#
include/assert.inc [Semisync master is off]
CREATE TABLE t1(c1 INT);
include/assert.inc [rpl_semi_sync_master_tx_waits should be 0]
#
# Semisync's status should still be OFF, because there is only one slave
#
[connection server_2]
include/install_semisync_slave.inc
[connection server_1]
include/assert.inc [ should be 2]
INSERT INTO t1 VALUES(1);
include/assert.inc [Semisync master is off]
#
# Verify 'SET GLOBAL rpl_semi_sync_master_enabled= ON' sets semisync
# master's status correctly when there are enough semisync slaves.
#
[connection server_3]
include/install_semisync_slave.inc
[connection server_1]
SET GLOBAL rpl_semi_sync_master_enabled= OFF;
include/assert.inc [Semisync master is off]
SET GLOBAL rpl_semi_sync_master_enabled= ON;
include/assert.inc [Semisync master is on]
DROP TABLE t1;
#
# Verify ack_receiver thread is created
#
include/assert.inc [ack receiver thread is created;]
CREATE TABLE t1(c1 INT);
include/rpl_sync.inc
#
# Verify semisync replication works well
#
[connection server_1]
include/assert.inc [Semisync master is on]
DROP TABLE t1;
include/rpl_sync.inc
[connection server_2]
include/uninstall_semisync_slave.inc
[connection server_3]
include/uninstall_semisync_slave.inc
include/rpl_end.inc
