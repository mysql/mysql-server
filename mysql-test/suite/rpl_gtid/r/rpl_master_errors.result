include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
call mtr.add_suppression(".* You need to use --log-bin to make .* work.");
CALL mtr.add_suppression("Failed to open log");
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION= 0;
include/start_slave.inc
[connection master]
#
# Error: binlog file doesn't exist
#
#
# Error: master doesn't set server_id
#
# Adding debug point 'simulate_no_server_id' to @@GLOBAL.debug
#
# Error: Error while sending faked rotate event
#
# Adding debug point 'simulate_send_error' to @@GLOBAL.debug
# Removing debug point 'simulate_no_server_id' from @@GLOBAL.debug
# Removing debug point 'simulate_send_error' from @@GLOBAL.debug
#
# Error: Fail to open binlog file
#
FLUSH BINARY LOGS;
#
# Error: Could not find Format_Description_Event
#
#
# Error: Fail on reading Previous_gtid_log_event
#
#
# Error: Fail on reading event
#
#
# Error: Client requests from a position less than 4
#
[connection slave]
include/stop_slave.inc
# Adding debug point 'request_source_log_pos_3' to @@GLOBAL.debug
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
call mtr.add_suppression(".* Got fatal error 1236 from master when reading data from binary log");
include/wait_for_slave_io_error.inc [errno=13114]
# Removing debug point 'request_source_log_pos_3' from @@GLOBAL.debug
#
# Error: Binary log is not open on master
#
include/rpl_restart_server.inc [server_number=1 parameters: --disable-log-bin --gtid-mode=off]
#
# Error: master's gtid_mode is not on
#
include/rpl_restart_server.inc [server_number=1 parameters: --gtid-mode=off]
include/rpl_restart_server.inc [server_number=1]
#
# Error: Dump thread is killed
#
[connection master]
RESET MASTER;
CREATE TABLE t1(c1 INT);
# Adding debug point 'simulate_kill_dump' to @@GLOBAL.debug
#
# Error: Fail to send heartbeat with an net error
#
INSERT INTO t1 VALUES(1);
# Adding debug point 'simulate_flush_error' to @@GLOBAL.debug
# Removing debug point 'simulate_kill_dump' from @@GLOBAL.debug
# Removing debug point 'simulate_flush_error' from @@GLOBAL.debug
DROP TABLE t1;
[connection slave]
RESET MASTER;
RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/start_slave.inc
include/rpl_end.inc
