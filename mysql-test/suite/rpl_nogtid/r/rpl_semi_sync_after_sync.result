include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/install_semisync.inc
SET GLOBAL binlog_error_action = IGNORE_ERROR;
CREATE TABLE t1(c1 INT) ENGINE=InnoDB;
include/sync_slave_sql_with_master.inc
#
# Test on AFTER_SYNC wait point
# #######################################################################
# Case 1: Single statement transaction
#
[connection server_1]
show status like "rpl_semi_sync_master_yes_tx";
Variable_name	Value
Rpl_semi_sync_master_yes_tx	1
SET DEBUG_SYNC= "after_call_after_sync_observer SIGNAL after_sync_done WAIT_FOR end";
INSERT INTO t1 VALUES(1);;
[connection master]
SET DEBUG_SYNC= "now WAIT_FOR after_sync_done";
show status like "rpl_semi_sync_master_yes_tx";
Variable_name	Value
Rpl_semi_sync_master_yes_tx	2
include/assert.inc [rpl_semi_sync_master_yes_tx increased 1]
include/assert.inc [Table is empty right now.]
include/sync_slave_sql_with_master.inc
include/assert.inc [Table has 1 record]
[connection master]
SET DEBUG_SYNC= "now SIGNAL end";
[connection server_1]
include/assert.inc [rpl_semi_sync_master_yes_tx increased only 1]
TRUNCATE t1;
#
# Test on AFTER_SYNC wait point
# #######################################################################
# Case 2: Real transaction
#
[connection server_1]
SET DEBUG_SYNC= "after_call_after_sync_observer SIGNAL after_sync_done WAIT_FOR end";
BEGIN;
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
COMMIT;
[connection master]
SET DEBUG_SYNC= "now WAIT_FOR after_sync_done";
include/assert.inc [rpl_semi_sync_master_yes_tx increased 1]
include/assert.inc [Table is empty right now.]
include/sync_slave_sql_with_master.inc
include/assert.inc [Table has two records]
[connection master]
SET DEBUG_SYNC= "now SIGNAL end";
[connection server_1]
include/assert.inc [rpl_semi_sync_master_yes_tx increased only 1]
TRUNCATE t1;
#
# Test on AFTER_COMMIT wait point
# #######################################################################
# Verify after_sync hook will not be executed
#
SET GLOBAL rpl_semi_sync_master_wait_point= AFTER_COMMIT;
SET DEBUG_SYNC= "before_call_after_commit_observer SIGNAL engine_commit_done
                 WAIT_FOR end";
INSERT INTO t1 VALUES(1);;
[connection master]
SET DEBUG_SYNC= "now WAIT_FOR engine_commit_done";
include/assert.inc [Table is empty right now.]
include/assert.inc [rpl_semi_sync_master_yes_tx was not increased]
include/assert.inc [rpl_semi_sync_master_no_tx was not increased]
SET DEBUG_SYNC= "now SIGNAL end";
[connection server_1]
SET DEBUG_SYNC= "RESET";
[connection master]
SET DEBUG_SYNC= "RESET";
SET GLOBAL rpl_semi_sync_master_wait_point= AFTER_SYNC;
TRUNCATE t1;
include/sync_slave_sql_with_master.inc
[connection master]
SET GLOBAL binlog_order_commits = OFF;
#
# Test on AFTER_SYNC wait point
# #######################################################################
# Case 1: Single statement transaction
#
[connection server_1]
show status like "rpl_semi_sync_master_yes_tx";
Variable_name	Value
Rpl_semi_sync_master_yes_tx	7
SET DEBUG_SYNC= "after_call_after_sync_observer SIGNAL after_sync_done WAIT_FOR end";
INSERT INTO t1 VALUES(1);;
[connection master]
SET DEBUG_SYNC= "now WAIT_FOR after_sync_done";
show status like "rpl_semi_sync_master_yes_tx";
Variable_name	Value
Rpl_semi_sync_master_yes_tx	8
include/assert.inc [rpl_semi_sync_master_yes_tx increased 1]
include/assert.inc [Table is empty right now.]
include/sync_slave_sql_with_master.inc
include/assert.inc [Table has 1 record]
[connection master]
SET DEBUG_SYNC= "now SIGNAL end";
[connection server_1]
include/assert.inc [rpl_semi_sync_master_yes_tx increased only 1]
TRUNCATE t1;
#
# Test on AFTER_SYNC wait point
# #######################################################################
# Case 2: Real transaction
#
[connection server_1]
SET DEBUG_SYNC= "after_call_after_sync_observer SIGNAL after_sync_done WAIT_FOR end";
BEGIN;
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
COMMIT;
[connection master]
SET DEBUG_SYNC= "now WAIT_FOR after_sync_done";
include/assert.inc [rpl_semi_sync_master_yes_tx increased 1]
include/assert.inc [Table is empty right now.]
include/sync_slave_sql_with_master.inc
include/assert.inc [Table has two records]
[connection master]
SET DEBUG_SYNC= "now SIGNAL end";
[connection server_1]
include/assert.inc [rpl_semi_sync_master_yes_tx increased only 1]
TRUNCATE t1;
#
# Test on AFTER_COMMIT wait point
# #######################################################################
# Verify after_sync hook will not be executed
#
SET GLOBAL rpl_semi_sync_master_wait_point= AFTER_COMMIT;
SET DEBUG_SYNC= "before_call_after_commit_observer SIGNAL engine_commit_done
                 WAIT_FOR end";
INSERT INTO t1 VALUES(1);;
[connection master]
SET DEBUG_SYNC= "now WAIT_FOR engine_commit_done";
include/assert.inc [Table is empty right now.]
include/assert.inc [rpl_semi_sync_master_yes_tx was not increased]
include/assert.inc [rpl_semi_sync_master_no_tx was not increased]
SET DEBUG_SYNC= "now SIGNAL end";
[connection server_1]
SET DEBUG_SYNC= "RESET";
[connection master]
SET DEBUG_SYNC= "RESET";
SET GLOBAL rpl_semi_sync_master_wait_point= AFTER_SYNC;
TRUNCATE t1;
include/sync_slave_sql_with_master.inc
[connection master]
SET GLOBAL binlog_order_commits= 1;
#
# For coverage test, simulate after_sync hook error
#
SET @saved_debug= @@GLOBAL.debug;
SET GLOBAL debug='d,simulate_after_sync_hook_error';
INSERT INTO t1 VALUES(1);
SELECT * FROM t1;
c1
1
SET GLOBAL debug= @saved_debug;
CALL mtr.add_suppression("Failed to run 'after_sync' hook");
include/rpl_restart_server.inc [server_number=1]
[connection slave]
include/start_slave_io.inc
[connection master]
CALL mtr.add_suppression("An error occurred during sync stage of the commit.");
DROP TABLE t1;
include/uninstall_semisync.inc
include/rpl_end.inc
