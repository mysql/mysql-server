include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CALL mtr.add_suppression("Using ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS implies limitations on the replication topology");
CREATE TABLE t(s INT);
INSERT INTO t VALUES(10);
# TC1. Test with ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS = LOCAL
[connection slave]
CHANGE REPLICATION SOURCE TO ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS = LOCAL, SOURCE_AUTO_POSITION = 0;
Warnings:
Note	4017	Using ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS creates limitations on the replication topology - you cannot fail over between downstream and upstream servers. Only use this option if it is not possible to enable GTIDs on the source, for instance, because of lack of permissions. If possible, use the procedure for enabling GTID transactions online instead, as described in the documentation.
include/start_slave.inc
include/wait_for_slave_param.inc [Slave_SQL_Running_State]
[connection master]
DROP TABLE t;
include/sync_slave_sql_with_master.inc
include/gtid_step_assert.inc [count=4, only_count=0]
include/stop_slave.inc
[connection master]
CREATE TABLE t(s INT);
INSERT INTO t VALUES(10);
# TC2. Test with ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS = <UUID>
[connection slave]
CHANGE REPLICATION SOURCE TO ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", SOURCE_AUTO_POSITION = 0;
Warnings:
Note	4017	Using ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS creates limitations on the replication topology - you cannot fail over between downstream and upstream servers. Only use this option if it is not possible to enable GTIDs on the source, for instance, because of lack of permissions. If possible, use the procedure for enabling GTID transactions online instead, as described in the documentation.
include/start_slave.inc
include/wait_for_slave_param.inc [Slave_SQL_Running_State]
[connection master]
DROP TABLE t;
include/sync_slave_sql_with_master.inc
include/gtid_step_assert.inc [count=3, only_count=0]
# TC3. Test with slave_sql_skip_counter
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS = OFF;
[connection master]
CREATE TABLE t(s INT);
INSERT INTO t VALUES(10);
include/save_binlog_position.inc
INSERT INTO t VALUES(20);
[connection slave]
include/start_slave_io.inc
[connection master]
include/sync_slave_io_with_master.inc
START SLAVE SQL_THREAD UNTIL SOURCE_LOG_FILE = "BINLOG_FILE", SOURCE_LOG_POS = BINLOG_POS;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/stop_slave.inc
SET GLOBAL sql_replica_skip_counter = 1;
Warnings:
Warning	4016	The value of sql_replica_skip_counter will only take effect for channels running with ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS <> OFF.
include/start_slave.inc
include/wait_for_slave_param.inc [Slave_SQL_Running_State]
include/diff_tables.inc [master:t, slave:t]
[connection master]
DROP TABLE t;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS = LOCAL;
Warnings:
Note	4017	Using ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS creates limitations on the replication topology - you cannot fail over between downstream and upstream servers. Only use this option if it is not possible to enable GTIDs on the source, for instance, because of lack of permissions. If possible, use the procedure for enabling GTID transactions online instead, as described in the documentation.
include/start_slave.inc
[connection master]
CREATE TABLE t1 (a INT);
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET GLOBAL sql_replica_skip_counter=1;
Warnings:
Warning	4016	The value of sql_replica_skip_counter will only take effect for channels running with ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS <> OFF.
include/start_slave.inc
[connection master]
INSERT INTO t1 VALUES (20);
INSERT INTO t1 VALUES (30);
include/sync_slave_sql_with_master.inc
include/assert.inc [There should only be one row in t1]
include/assert.inc [Table t1 should contain the value 30]
[connection master]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
