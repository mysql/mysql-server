include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
==== Initialize ====
call mtr.add_suppression("Slave I/O for channel '': The replication receiver thread cannot start");
call mtr.add_suppression("Slave I/O for channel '': The slave IO thread stops");
call mtr.add_suppression("Slave I/O for channel '': Got fatal error 1236 from master when reading data from binary log: 'Cannot replicate GTID-transaction when @@GLOBAL.GTID_MODE = OFF");
call mtr.add_suppression("Slave I/O for channel '': Cannot replicate GTID-transaction when @@GLOBAL.GTID_MODE = OFF");
call mtr.add_suppression("GTID_LOG_EVENT or ANONYMOUS_GTID_LOG_EVENT is not expected in an event stream after a GTID_LOG_EVENT or an ANONYMOUS_GTID_LOG_EVENT.");
call mtr.add_suppression("An unexpected event sequence was detected by the IO thread while queuing the event received from master");
call mtr.add_suppression("Detected misconfiguration: replication channel '' was configured with AUTO_POSITION = 1, but the server was started with --gtid-mode=off.");
call mtr.add_suppression("Slave I/O for channel '': Relay log write failure: could not queue event from master");
call mtr.add_suppression("Got fatal error 1236 from master when reading data from binary log: 'Cannot replicate anonymous transaction when @@GLOBAL.GTID_MODE = ON");
call mtr.add_suppression("Slave I/O for channel '': Cannot replicate anonymous transaction when @@GLOBAL.GTID_MODE = ON");
call mtr.add_suppression("Slave I/O for channel '': Cannot replicate anonymous transaction when AUTO_POSITION = 1");
call mtr.add_suppression("Got fatal error 1236 from master when reading data from binary log: 'Cannot replicate anonymous transaction when AUTO_POSITION = 1,");
call mtr.add_suppression("Slave I/O for channel '': The master uses an unknown GTID_MODE 'on_something'. Treating it as 'ON'.");
call mtr.add_suppression("Slave I/O for channel '': The master uses an unknown GTID_MODE 'off_something'. Treating it as 'OFF'.");
call mtr.add_suppression("Could not open .* for logging.*. Turning logging off for the whole duration of the MySQL server process.");
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
==== Checks performed at server start ====
---- gtid-mode=ON requires enforce-gtid-consistency ----
include/rpl_stop_server.inc [server_number=2]
include/assert_command_output.inc
include/rpl_start_server.inc [server_number=2 gtids=off]
==== Master-slave handshake checks ====
[connection master]
SET GLOBAL ENFORCE_GTID_CONSISTENCY = ON;
[connection slave]
SET GLOBAL ENFORCE_GTID_CONSISTENCY = ON;
include/rpl_set_gtid_mode.inc [0 on servers 1]
include/rpl_set_gtid_mode.inc [0 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
ERROR HY000: CHANGE MASTER TO MASTER_AUTO_POSITION = 1 cannot be executed because @@GLOBAL.GTID_MODE = OFF.
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [1 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [2 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [3 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [1 on servers 1]
include/rpl_set_gtid_mode.inc [0 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
ERROR HY000: CHANGE MASTER TO MASTER_AUTO_POSITION = 1 cannot be executed because @@GLOBAL.GTID_MODE = OFF.
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [1 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [2 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [3 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [2 on servers 1]
include/rpl_set_gtid_mode.inc [0 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
ERROR HY000: CHANGE MASTER TO MASTER_AUTO_POSITION = 1 cannot be executed because @@GLOBAL.GTID_MODE = OFF.
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [1 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [2 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [3 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [3 on servers 1]
include/rpl_set_gtid_mode.inc [0 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
ERROR HY000: CHANGE MASTER TO MASTER_AUTO_POSITION = 1 cannot be executed because @@GLOBAL.GTID_MODE = OFF.
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [1 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [2 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
include/rpl_set_gtid_mode.inc [3 on servers 2]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
#
# Test for ER_CANT_REPLICATE_GTID_WITH_GTID_MODE_OFF
#   eng "Cannot replicate GTID-transaction when GTID_MODE = OFF, at file %.512s, position %lld. Found a Gtid_log_event when @@GLOBAL.GTID_MODE = OFF.
#
#
# Case 1: Error on Master(sender thread)
#
include/rpl_set_gtid_mode.inc [ON on servers 1]
[connection master]
CREATE TABLE t1 (a int);
include/rpl_set_gtid_mode.inc [OFF on servers 1,2]
[connection slave]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13114  # ER_CANT_REPLICATE_GTID_WITH_GTID_MODE_OFF]
include/assert.inc [sender thread should report - Cannot replicate GTID-transaction when @@GLOBAL.GTID_MODE = OFF.]
include/assert_grep.inc [Receiver thread should report - Cannot replicate GTID-transaction when @@GLOBAL.GTID_MODE = OFF.]
[connection master]
DROP TABLE t1;
RESET MASTER;
[connection slave]
RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
#
# Case 2: Error on Slave(receiver thread)
#
include/rpl_set_gtid_mode.inc [ON on servers 1,2]
[connection slave]
include/start_slave.inc
include/rpl_set_gtid_mode.inc [OFF on servers 2]
[connection master]
CREATE TABLE t2(a int);
[connection slave]
include/wait_for_slave_io_error.inc [errno=13122 # ER_SLAVE_RELAY_LOG_WRITE_FAILURE]
include/assert_grep.inc [Receiver thread should report - Cannot replicate GTID-transaction when @@GLOBAL.GTID_MODE = OFF.]
[connection master]
DROP TABLE t2;
[connection slave]
include/rpl_set_gtid_mode.inc [ON on servers 2]
include/start_slave.inc
#
#  Warning: "Detected misconfiguration: replication channel '%.192s' was configured with AUTO_POSITION = 1, but the server was started with --gtid-mode=off.  Either reconfigure replication using CHANGE MASTER TO MASTER_AUTO_POSITION = 0 FOR CHANNEL '%.192s', or change GTID_MODE to some value other than OFF, before starting the slave receiver thread."
#
include/rpl_set_gtid_mode.inc [ON on servers 1,2]
[connection slave]
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION= 1;
include/rpl_restart_server.inc [server_number=2 gtids=off]
SET GLOBAL ENFORCE_GTID_CONSISTENCY = ON;
include/assert_grep.inc [While AUTO_POSITION is set, attempt to restart the slave with gtid-mode= off to get ER_STARTING_WITH_GTID_MODE_OFF_AND_AUTO_POSITION.]
#
# ER_CANT_USE_AUTO_POSITION_WITH_GTID_MODE_OFF
#   eng "The replication receiver thread for channel '%s' cannot start in AUTO_POSITION mode: this server uses GTID_MODE = OFF."
#
START SLAVE IO_THREAD;
ERROR HY000: The replication receiver thread for channel '' cannot start in AUTO_POSITION mode: this server uses @@GLOBAL.GTID_MODE = OFF.
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION= 0;
#
# ER_CANT_REPLICATE_ANONYMOUS_WITH_GTID_MODE_ON
#   eng "Cannot replicate anonymous transaction when GTID_MODE = ON, at file %.512s, position %lld."
#
# Case 1: Error on master(sender thread)
#
include/rpl_set_gtid_mode.inc [OFF on servers 1]
[connection master]
CREATE TABLE t3(a int);
include/rpl_set_gtid_mode.inc [ON on servers 1,2]
[connection slave]
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13114 # ER_CANT_REPLICATE_ANONYMOUS_WITH_GTID_MODE_ON]
include/assert.inc [sender thread should report - Cannot replicate anonymous transaction when @@GLOBAL.GTID_MODE = ON.]
include/assert_grep.inc [While gtid-mode=on, slave expects an anonymous event to get ER_CANT_REPLICATE_ANONYMOUS_WITH_GTID_MODE_ON.]
[connection master]
DROP TABLE t3;
include/rpl_reset.inc
Warnings:
Note	3084	Replication thread(s) for channel '' are already stopped.
#
# Case 2: Error on slave(receiver thread)
#
include/rpl_set_gtid_mode.inc [OFF on servers 1]
[connection master]
CREATE TABLE t4(a int);
[connection slave]
include/wait_for_slave_io_error.inc [errno=13122 # ER_SLAVE_RELAY_LOG_WRITE_FAILURE]
include/assert_grep.inc [While gtid-mode=on, slave expects an anonymous event to get ER_CANT_REPLICATE_ANONYMOUS_WITH_GTID_MODE_ON.]
include/stop_slave_sql.inc
include/rpl_set_gtid_mode.inc [OFF on servers 2]
[connection master]
DROP TABLE t4;
RESET MASTER;
[connection slave]
RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/start_slave.inc
[connection master]
include/sync_slave_sql_with_master.inc
#
# ER_CANT_REPLICATE_ANONYMOUS_WITH_AUTO_POSITION
#   eng "Cannot replicate anonymous transaction when AUTO_POSITION = 1, at file %.512s, position %lld."
#
#
# Case 1: Error on master(sender thread).
#
include/rpl_set_gtid_mode.inc [ON on servers 1,2]
[connection master]
CREATE TABLE t5(a int);
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
[connection master]
INSERT INTO t5 VALUES(2);
include/rpl_set_gtid_mode.inc [ON_PERMISSIVE on servers 1]
SET @@SESSION.GTID_NEXT = 'ANONYMOUS';
INSERT INTO t5 VALUES(1);
include/rpl_set_gtid_mode.inc [ON on servers 1]
include/rpl_set_gtid_mode.inc [ON on servers 2]
[connection slave]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION= 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13114 # ER_CANT_REPLICATE_ANONYMOUS_WITH_AUTO_POSITION]
include/assert.inc [sender thread should report - Cannot replicate anonymous transaction when AUTO_POSITION = 1.]
include/assert_grep.inc [While AUTO_POSITION is enabled, Master sends an anonymous transaction resulting into ER_CANT_REPLICATE_ANONYMOUS_WITH_AUTO_POSITION.]
include/rpl_restart_server.inc [server_number=1 gtids=off]
[connection master]
SET GLOBAL ENFORCE_GTID_CONSISTENCY = ON;
[connection slave]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION= 0;
include/rpl_set_gtid_mode.inc [OFF on servers 2]
[connection master]
DROP TABLE t5;
RESET MASTER;
[connection slave]
DROP TABLE t5;
RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/start_slave.inc
[connection master]
include/sync_slave_sql_with_master.inc
#
# Case 2: Error on Slave(receiver thread).
#
[connection master]
SET @debug_saved= @@GLOBAL.DEBUG;
SET GLOBAL DEBUG= "+d,skip_sender_anon_autoposition_error";
include/rpl_set_gtid_mode.inc [ON on servers 1,2]
[connection master]
CREATE TABLE t6(a int);
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
[connection master]
INSERT INTO t6 VALUES(2);
include/rpl_set_gtid_mode.inc [ON_PERMISSIVE on servers 1]
SET @@SESSION.GTID_NEXT = 'ANONYMOUS';
INSERT INTO t6 VALUES(1);
include/rpl_set_gtid_mode.inc [ON on servers 1]
include/rpl_set_gtid_mode.inc [ON on servers 2]
[connection slave]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION= 1;
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13122 # ER_SLAVE_RELAY_LOG_WRITE_FAILURE]
include/assert_grep.inc [While AUTO_POSITION is enabled, Master sends an anonymous transaction resulting into ER_CANT_REPLICATE_ANONYMOUS_WITH_AUTO_POSITION.]
[connection slave]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION= 0;
include/rpl_set_gtid_mode.inc [OFF on servers 1,2]
[connection master]
SET @@GLOBAL.DEBUG= @debug_saved;
SET DEBUG_SYNC= 'RESET';
DROP TABLE t6;
RESET MASTER;
[connection slave]
RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/start_slave.inc
[connection master]
include/sync_slave_sql_with_master.inc
#
# Verify the behaviour when rotate fails while setting gtid_mode.
#
[connection slave]
include/start_slave.inc
Warnings:
Note	3083	Replication thread(s) for channel '' are already runnning.
[connection master]
call mtr.add_suppression("Could not open * for logging");
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET @debug_saved= @@GLOBAL.DEBUG;
SET GLOBAL DEBUG= "+d,fault_injection_new_file_rotate_event";
SET GLOBAL binlog_error_action= IGNORE_ERROR;
SET @@GLOBAL.GTID_MODE= OFF_PERMISSIVE;
ERROR HY000: Error writing file 'slave-bin' (errno: 2 - No such file or directory)
SET @@GLOBAL.GTID_MODE= OFF;
SET @@GLOBAL.DEBUG= @debug_saved;
SET GLOBAL binlog_error_action= ABORT_SERVER;
include/rpl_restart_server.inc [server_number=1 gtids=off]
[connection master]
SET GLOBAL ENFORCE_GTID_CONSISTENCY = ON;
#
# ER_CANT_SET_GTID_NEXT_TO_ANONYMOUS_WHEN_GTID_MODE_IS_ON
# generated when re-acquiring anonymous ownership
#
[connection master]
SET GTID_NEXT='ANONYMOUS';
CREATE TABLE t7(a int);
include/rpl_set_gtid_mode.inc [ON on servers 1]
INSERT INTO t7 values (1);
ERROR HY000: @@SESSION.GTID_NEXT cannot be set to ANONYMOUS when @@GLOBAL.GTID_MODE = ON.
SET GTID_NEXT='AUTOMATIC';
DROP TABLE t7;
RESET MASTER;
include/rpl_set_gtid_mode.inc [OFF on servers 1]
#
# Error generated if master has an unknown gtid_mode
#
include/stop_slave.inc
Warnings:
Note	3084	Replication thread(s) for channel '' are already stopped.
RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
SET @@GLOBAL.DEBUG= '+d,simulate_source_has_unknown_gtid_mode';
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
Last_IO_Error = 'The slave IO thread stops because the master has an unknown @@GLOBAL.GTID_MODE 'Krakel Spektakel'.'
SET @@GLOBAL.DEBUG= @debug_saved;
SET @@GLOBAL.GTID_MODE = OFF_PERMISSIVE;
RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
#
# ER_CANT_SET_GTID_MODE generated because AUTO_POSITION = 1.
#
include/rpl_set_gtid_mode.inc [ON on servers 1]
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 1;
SET @@GLOBAL.GTID_MODE = OFF;
ERROR HY000: SET @@GLOBAL.GTID_MODE = OFF is not allowed because replication channel '' is configured in AUTO_POSITION mode. Execute CHANGE MASTER TO MASTER_AUTO_POSITION = 0 FOR CHANNEL '' before you set @@GLOBAL.GTID_MODE = OFF..
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0;
SET @@GLOBAL.GTID_MODE = OFF;
include/rpl_set_gtid_mode.inc [OFF on servers 1]
[connection master]
SET GLOBAL ENFORCE_GTID_CONSISTENCY = OFF;
[connection slave]
SET GLOBAL ENFORCE_GTID_CONSISTENCY = OFF;
include/rpl_end.inc
