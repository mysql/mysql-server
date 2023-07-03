include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#### Initialize ####
include/suppress_messages.inc
# Connection 1 suppresses message <Master command COM_REGISTER_SLAVE failed: failed>.
# Connection 1 suppresses message <Master command COM_BINLOG_DUMP failed: failed>.
# Connection 1 suppresses message <Failed on request_dump.. for channel>.
# Connection 2 suppresses message <Master command COM_REGISTER_SLAVE failed: failed>.
# Connection 2 suppresses message <Master command COM_BINLOG_DUMP failed: failed>.
# Connection 2 suppresses message <Failed on request_dump.. for channel>.

#### Testing terminology_use_previous = 0 ####
SET @@session.terminology_use_previous = 0;
Warnings:
Warning	1287	'@@terminology_use_previous' is deprecated and will be removed in a future release.

---- replica_waiting_to_reconnect after failed_registration ----
# Adding debug point 'simulate_reconnect_after_failed_registration' to @@GLOBAL.debug
# Adding debug point 'hang_in_stage_replica_waiting_to_reconnect' to @@GLOBAL.debug
START REPLICA IO_THREAD;
SELECT STATE FROM INFORMATION_SCHEMA.PROCESSLIST WHERE STATE LIKE '%connect%after a failed%';
STATE
Waiting to reconnect after a failed registration on source
SELECT STATE FROM performance_schema.processlist WHERE STATE LIKE '%connect%after a failed%';
STATE
Waiting to reconnect after a failed registration on source
SELECT PROCESSLIST_STATE FROM performance_schema.threads WHERE PROCESSLIST_STATE LIKE '%connect%after a failed%';
PROCESSLIST_STATE
Waiting to reconnect after a failed registration on source
Slave_IO_State = 'Waiting to reconnect after a failed registration on source'
# Removing debug point 'simulate_reconnect_after_failed_registration' from @@GLOBAL.debug
# Removing debug point 'hang_in_stage_replica_waiting_to_reconnect' from @@GLOBAL.debug
KILL THREAD_ID_OF_RECEIVER;
include/wait_for_slave_io_to_stop.inc

---- replica_reconnecting after failed_registration ----
# Adding debug point 'simulate_reconnect_after_failed_registration' to @@GLOBAL.debug
# Adding debug point 'hang_in_stage_replica_reconnecting' to @@GLOBAL.debug
START REPLICA IO_THREAD;
SELECT STATE FROM INFORMATION_SCHEMA.PROCESSLIST WHERE STATE LIKE '%connect%after a failed%';
STATE
Reconnecting after a failed registration on source
SELECT STATE FROM performance_schema.processlist WHERE STATE LIKE '%connect%after a failed%';
STATE
Reconnecting after a failed registration on source
SELECT PROCESSLIST_STATE FROM performance_schema.threads WHERE PROCESSLIST_STATE LIKE '%connect%after a failed%';
PROCESSLIST_STATE
Reconnecting after a failed registration on source
Slave_IO_State = 'Reconnecting after a failed registration on source'
# Removing debug point 'simulate_reconnect_after_failed_registration' from @@GLOBAL.debug
# Removing debug point 'hang_in_stage_replica_reconnecting' from @@GLOBAL.debug
KILL THREAD_ID_OF_RECEIVER;
include/wait_for_slave_io_to_stop.inc

---- replica_waiting_to_reconnect after failed_binlog_dump ----
# Adding debug point 'simulate_reconnect_after_failed_binlog_dump' to @@GLOBAL.debug
# Adding debug point 'hang_in_stage_replica_waiting_to_reconnect' to @@GLOBAL.debug
START REPLICA IO_THREAD;
SELECT STATE FROM INFORMATION_SCHEMA.PROCESSLIST WHERE STATE LIKE '%connect%after a failed%';
STATE
Waiting to reconnect after a failed binlog dump request
SELECT STATE FROM performance_schema.processlist WHERE STATE LIKE '%connect%after a failed%';
STATE
Waiting to reconnect after a failed binlog dump request
SELECT PROCESSLIST_STATE FROM performance_schema.threads WHERE PROCESSLIST_STATE LIKE '%connect%after a failed%';
PROCESSLIST_STATE
Waiting to reconnect after a failed binlog dump request
Slave_IO_State = 'Waiting to reconnect after a failed binlog dump request'
# Removing debug point 'simulate_reconnect_after_failed_binlog_dump' from @@GLOBAL.debug
# Removing debug point 'hang_in_stage_replica_waiting_to_reconnect' from @@GLOBAL.debug
KILL THREAD_ID_OF_RECEIVER;
include/wait_for_slave_io_to_stop.inc

---- replica_reconnecting after failed_binlog_dump ----
# Adding debug point 'simulate_reconnect_after_failed_binlog_dump' to @@GLOBAL.debug
# Adding debug point 'hang_in_stage_replica_reconnecting' to @@GLOBAL.debug
START REPLICA IO_THREAD;
SELECT STATE FROM INFORMATION_SCHEMA.PROCESSLIST WHERE STATE LIKE '%connect%after a failed%';
STATE
Reconnecting after a failed binlog dump request
SELECT STATE FROM performance_schema.processlist WHERE STATE LIKE '%connect%after a failed%';
STATE
Reconnecting after a failed binlog dump request
SELECT PROCESSLIST_STATE FROM performance_schema.threads WHERE PROCESSLIST_STATE LIKE '%connect%after a failed%';
PROCESSLIST_STATE
Reconnecting after a failed binlog dump request
Slave_IO_State = 'Reconnecting after a failed binlog dump request'
# Removing debug point 'simulate_reconnect_after_failed_binlog_dump' from @@GLOBAL.debug
# Removing debug point 'hang_in_stage_replica_reconnecting' from @@GLOBAL.debug
KILL THREAD_ID_OF_RECEIVER;
include/wait_for_slave_io_to_stop.inc

---- replica_waiting_to_reconnect after failed_event_read ----
# Adding debug point 'simulate_reconnect_after_failed_event_read' to @@GLOBAL.debug
# Adding debug point 'hang_in_stage_replica_waiting_to_reconnect' to @@GLOBAL.debug
START REPLICA IO_THREAD;
SELECT STATE FROM INFORMATION_SCHEMA.PROCESSLIST WHERE STATE LIKE '%connect%after a failed%';
STATE
Waiting to reconnect after a failed source event read
SELECT STATE FROM performance_schema.processlist WHERE STATE LIKE '%connect%after a failed%';
STATE
Waiting to reconnect after a failed source event read
SELECT PROCESSLIST_STATE FROM performance_schema.threads WHERE PROCESSLIST_STATE LIKE '%connect%after a failed%';
PROCESSLIST_STATE
Waiting to reconnect after a failed source event read
Slave_IO_State = 'Waiting to reconnect after a failed source event read'
# Removing debug point 'simulate_reconnect_after_failed_event_read' from @@GLOBAL.debug
# Removing debug point 'hang_in_stage_replica_waiting_to_reconnect' from @@GLOBAL.debug
KILL THREAD_ID_OF_RECEIVER;
include/wait_for_slave_io_to_stop.inc

---- replica_reconnecting after failed_event_read ----
# Adding debug point 'simulate_reconnect_after_failed_event_read' to @@GLOBAL.debug
# Adding debug point 'hang_in_stage_replica_reconnecting' to @@GLOBAL.debug
START REPLICA IO_THREAD;
SELECT STATE FROM INFORMATION_SCHEMA.PROCESSLIST WHERE STATE LIKE '%connect%after a failed%';
STATE
Reconnecting after a failed source event read
SELECT STATE FROM performance_schema.processlist WHERE STATE LIKE '%connect%after a failed%';
STATE
Reconnecting after a failed source event read
SELECT PROCESSLIST_STATE FROM performance_schema.threads WHERE PROCESSLIST_STATE LIKE '%connect%after a failed%';
PROCESSLIST_STATE
Reconnecting after a failed source event read
Slave_IO_State = 'Reconnecting after a failed source event read'
# Removing debug point 'simulate_reconnect_after_failed_event_read' from @@GLOBAL.debug
# Removing debug point 'hang_in_stage_replica_reconnecting' from @@GLOBAL.debug
KILL THREAD_ID_OF_RECEIVER;
include/wait_for_slave_io_to_stop.inc

#### Testing terminology_use_previous = 1 ####
SET @@session.terminology_use_previous = 1;
Warnings:
Warning	1287	'@@terminology_use_previous' is deprecated and will be removed in a future release.

---- replica_waiting_to_reconnect after failed_registration ----
# Adding debug point 'simulate_reconnect_after_failed_registration' to @@GLOBAL.debug
# Adding debug point 'hang_in_stage_replica_waiting_to_reconnect' to @@GLOBAL.debug
START REPLICA IO_THREAD;
SELECT STATE FROM INFORMATION_SCHEMA.PROCESSLIST WHERE STATE LIKE '%connect%after a failed%';
STATE
Waiting to reconnect after a failed registration on master
SELECT STATE FROM performance_schema.processlist WHERE STATE LIKE '%connect%after a failed%';
STATE
Waiting to reconnect after a failed registration on master
SELECT PROCESSLIST_STATE FROM performance_schema.threads WHERE PROCESSLIST_STATE LIKE '%connect%after a failed%';
PROCESSLIST_STATE
Waiting to reconnect after a failed registration on master
Slave_IO_State = 'Waiting to reconnect after a failed registration on master'
# Removing debug point 'simulate_reconnect_after_failed_registration' from @@GLOBAL.debug
# Removing debug point 'hang_in_stage_replica_waiting_to_reconnect' from @@GLOBAL.debug
KILL THREAD_ID_OF_RECEIVER;
include/wait_for_slave_io_to_stop.inc

---- replica_reconnecting after failed_registration ----
# Adding debug point 'simulate_reconnect_after_failed_registration' to @@GLOBAL.debug
# Adding debug point 'hang_in_stage_replica_reconnecting' to @@GLOBAL.debug
START REPLICA IO_THREAD;
SELECT STATE FROM INFORMATION_SCHEMA.PROCESSLIST WHERE STATE LIKE '%connect%after a failed%';
STATE
Reconnecting after a failed registration on master
SELECT STATE FROM performance_schema.processlist WHERE STATE LIKE '%connect%after a failed%';
STATE
Reconnecting after a failed registration on master
SELECT PROCESSLIST_STATE FROM performance_schema.threads WHERE PROCESSLIST_STATE LIKE '%connect%after a failed%';
PROCESSLIST_STATE
Reconnecting after a failed registration on master
Slave_IO_State = 'Reconnecting after a failed registration on master'
# Removing debug point 'simulate_reconnect_after_failed_registration' from @@GLOBAL.debug
# Removing debug point 'hang_in_stage_replica_reconnecting' from @@GLOBAL.debug
KILL THREAD_ID_OF_RECEIVER;
include/wait_for_slave_io_to_stop.inc

---- replica_waiting_to_reconnect after failed_binlog_dump ----
# Adding debug point 'simulate_reconnect_after_failed_binlog_dump' to @@GLOBAL.debug
# Adding debug point 'hang_in_stage_replica_waiting_to_reconnect' to @@GLOBAL.debug
START REPLICA IO_THREAD;
SELECT STATE FROM INFORMATION_SCHEMA.PROCESSLIST WHERE STATE LIKE '%connect%after a failed%';
STATE
Waiting to reconnect after a failed binlog dump request
SELECT STATE FROM performance_schema.processlist WHERE STATE LIKE '%connect%after a failed%';
STATE
Waiting to reconnect after a failed binlog dump request
SELECT PROCESSLIST_STATE FROM performance_schema.threads WHERE PROCESSLIST_STATE LIKE '%connect%after a failed%';
PROCESSLIST_STATE
Waiting to reconnect after a failed binlog dump request
Slave_IO_State = 'Waiting to reconnect after a failed binlog dump request'
# Removing debug point 'simulate_reconnect_after_failed_binlog_dump' from @@GLOBAL.debug
# Removing debug point 'hang_in_stage_replica_waiting_to_reconnect' from @@GLOBAL.debug
KILL THREAD_ID_OF_RECEIVER;
include/wait_for_slave_io_to_stop.inc

---- replica_reconnecting after failed_binlog_dump ----
# Adding debug point 'simulate_reconnect_after_failed_binlog_dump' to @@GLOBAL.debug
# Adding debug point 'hang_in_stage_replica_reconnecting' to @@GLOBAL.debug
START REPLICA IO_THREAD;
SELECT STATE FROM INFORMATION_SCHEMA.PROCESSLIST WHERE STATE LIKE '%connect%after a failed%';
STATE
Reconnecting after a failed binlog dump request
SELECT STATE FROM performance_schema.processlist WHERE STATE LIKE '%connect%after a failed%';
STATE
Reconnecting after a failed binlog dump request
SELECT PROCESSLIST_STATE FROM performance_schema.threads WHERE PROCESSLIST_STATE LIKE '%connect%after a failed%';
PROCESSLIST_STATE
Reconnecting after a failed binlog dump request
Slave_IO_State = 'Reconnecting after a failed binlog dump request'
# Removing debug point 'simulate_reconnect_after_failed_binlog_dump' from @@GLOBAL.debug
# Removing debug point 'hang_in_stage_replica_reconnecting' from @@GLOBAL.debug
KILL THREAD_ID_OF_RECEIVER;
include/wait_for_slave_io_to_stop.inc

---- replica_waiting_to_reconnect after failed_event_read ----
# Adding debug point 'simulate_reconnect_after_failed_event_read' to @@GLOBAL.debug
# Adding debug point 'hang_in_stage_replica_waiting_to_reconnect' to @@GLOBAL.debug
START REPLICA IO_THREAD;
SELECT STATE FROM INFORMATION_SCHEMA.PROCESSLIST WHERE STATE LIKE '%connect%after a failed%';
STATE
Waiting to reconnect after a failed master event read
SELECT STATE FROM performance_schema.processlist WHERE STATE LIKE '%connect%after a failed%';
STATE
Waiting to reconnect after a failed master event read
SELECT PROCESSLIST_STATE FROM performance_schema.threads WHERE PROCESSLIST_STATE LIKE '%connect%after a failed%';
PROCESSLIST_STATE
Waiting to reconnect after a failed master event read
Slave_IO_State = 'Waiting to reconnect after a failed master event read'
# Removing debug point 'simulate_reconnect_after_failed_event_read' from @@GLOBAL.debug
# Removing debug point 'hang_in_stage_replica_waiting_to_reconnect' from @@GLOBAL.debug
KILL THREAD_ID_OF_RECEIVER;
include/wait_for_slave_io_to_stop.inc

---- replica_reconnecting after failed_event_read ----
# Adding debug point 'simulate_reconnect_after_failed_event_read' to @@GLOBAL.debug
# Adding debug point 'hang_in_stage_replica_reconnecting' to @@GLOBAL.debug
START REPLICA IO_THREAD;
SELECT STATE FROM INFORMATION_SCHEMA.PROCESSLIST WHERE STATE LIKE '%connect%after a failed%';
STATE
Reconnecting after a failed master event read
SELECT STATE FROM performance_schema.processlist WHERE STATE LIKE '%connect%after a failed%';
STATE
Reconnecting after a failed master event read
SELECT PROCESSLIST_STATE FROM performance_schema.threads WHERE PROCESSLIST_STATE LIKE '%connect%after a failed%';
PROCESSLIST_STATE
Reconnecting after a failed master event read
Slave_IO_State = 'Reconnecting after a failed master event read'
# Removing debug point 'simulate_reconnect_after_failed_event_read' from @@GLOBAL.debug
# Removing debug point 'hang_in_stage_replica_reconnecting' from @@GLOBAL.debug
KILL THREAD_ID_OF_RECEIVER;
include/wait_for_slave_io_to_stop.inc

#### Clean up ####
SET @@session.terminology_use_previous = NONE;
Warnings:
Warning	1287	'@@terminology_use_previous' is deprecated and will be removed in a future release.
include/start_slave.inc
include/rpl_end.inc
