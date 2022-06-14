include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# Case 1: Verify source_pos_wait can block RESET SLAVE ALL
#
[connection slave]
SET debug_sync = "begin_source_pos_wait SIGNAL wait_func_begin WAIT_FOR wait_func_continue";
SELECT source_pos_wait("master-bin.000001", 10000);;
[connection slave1]
SET debug_sync = "now WAIT_FOR wait_func_begin";
include/stop_slave.inc
RESET SLAVE ALL;
[connection server_2]
SET debug_sync = "now signal wait_func_continue";
[connection slave1]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
SET debug_sync = 'RESET';
[connection slave]
source_pos_wait("master-bin.000001", 10000)
NULL
SET debug_sync = 'RESET';
[connection master]
include/save_binlog_position.inc
[connection slave]
CHANGE REPLICATION SOURCE TO SOURCE_LOG_FILE = "BINLOG_FILE",
SOURCE_LOG_POS = BINLOG_POS, SOURCE_USER='root',
SOURCE_HOST='127.0.0.1', SOURCE_PORT=MASTER_PORT;
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
#
# Case 2: Verify wait_for_gtid_set can block RESET SLAVE ALL
#
[connection slave]
include/start_slave.inc
SET debug_sync = "begin_wait_for_gtid_set SIGNAL wait_func_begin WAIT_FOR wait_func_continue";
SELECT wait_until_sql_thread_after_gtids(CONCAT(@@GLOBAL.server_uuid, ":100"));
[connection slave1]
SET debug_sync = "now WAIT_FOR wait_func_begin";
include/stop_slave.inc
RESET SLAVE ALL;
[connection server_2]
SET debug_sync = "now signal wait_func_continue";
[connection slave1]
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
SET debug_sync = 'RESET';
[connection slave]
wait_until_sql_thread_after_gtids(CONCAT(@@GLOBAL.server_uuid, ":100"))
NULL
Warnings:
Warning	1287	'WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS' is deprecated and will be removed in a future release. Please use WAIT_FOR_EXECUTED_GTID_SET instead
SET debug_sync = 'RESET';
[connection master]
include/save_binlog_position.inc
[connection slave]
CHANGE REPLICATION SOURCE TO SOURCE_LOG_FILE = "BINLOG_FILE",
SOURCE_LOG_POS = BINLOG_POS, SOURCE_USER='root',
SOURCE_HOST='127.0.0.1', SOURCE_PORT=MASTER_PORT;
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/rpl_end.inc
