include/rpl_init.inc [topology=1->4,2->4,3->4]
[connection server_4]
CALL mtr.add_suppression("Failed to open the relay log");
CALL mtr.add_suppression("Could not find target log file mentioned in relay log info in the index file");
CALL mtr.add_suppression(".*Failed to initialize the master info structure for channel.*");
CALL mtr.add_suppression("Failed to create or recover replication info repositories.");
CALL mtr.add_suppression("Slave failed to initialize relay log info structure from the repository");
CALL mtr.add_suppression("Failed to start slave threads for channel");
CALL mtr.add_suppression("Could not start slave for channel");
CALL mtr.add_suppression("file .* was not purged because it was being read by thread");
[connection server_1]
RESET MASTER;
[connection server_4]
CHANGE REPLICATION SOURCE TO SOURCE_USER='root', SOURCE_PORT= port, SOURCE_HOST='localhost' FOR CHANNEL '';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc [FOR CHANNEL '']
[connection server_1]
CREATE DATABASE db1;
CREATE TABLE db1.t1(i int);
DROP TABLE db1.t1;
DROP DATABASE db1;
include/sync_slave_sql_with_master.inc [FOR CHANNEL '']
[connection server_2]
RESET MASTER;
[connection server_4]
CHANGE REPLICATION SOURCE TO SOURCE_USER='root', SOURCE_PORT= port, SOURCE_HOST='localhost' FOR CHANNEL 'channel_2';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc [FOR CHANNEL 'channel_2']
[connection server_2]
CREATE DATABASE db2;
CREATE TABLE db2.t2(i int);
DROP TABLE db2.t2;
DROP DATABASE db2;
include/sync_slave_sql_with_master.inc [FOR CHANNEL 'channel_2']
[connection server_3]
RESET MASTER;
[connection server_4]
CHANGE REPLICATION SOURCE TO SOURCE_USER='root', SOURCE_PORT= port, SOURCE_HOST='localhost' FOR CHANNEL 'channel_3';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc [FOR CHANNEL 'channel_3']
[connection server_3]
CREATE DATABASE db3;
CREATE TABLE db3.t3(i int);
DROP TABLE db3.t3;
DROP DATABASE db3;
include/sync_slave_sql_with_master.inc [FOR CHANNEL 'channel_3']
===== Corrupting Channel ''.
[connection server_4]
===== Stop the server 4.
include/rpl_stop_server.inc [server_number=4]
===== Remove '' channel's relaylog file.
===== Start the server 4 with --skip-replica-start=0 option.
include/rpl_start_server.inc [server_number=4 parameters: --skip-replica-start=0]
===== Assert that all channels IO and SQL threads are in expected state.
include/assert.inc [Corrupted channel's IO thread should be in OFF state.]
include/assert.inc [Corrupted channel's SQL thread should be in OFF state.]
===== ONLY one channel is corrupted. Waiting until started channels be 2.
===== ONLY one channel is corrupted. Waiting until started channels be 2.
===== Executing few basic commands without 'FOR CHANNEL' clause.
===== STOP SLAVE without 'FOR CHANNEL' clause.
include/stop_slave.inc
Warnings:
Note	3084	Replication thread(s) for channel '' are already stopped.
=== Assert that all channels IO and SQL threads are in OFF state ===
include/assert.inc [All the channel's IO thread should be in OFF state.]
include/assert.inc [All the channel's SQL thread should be in OFF state.]
===== START SLAVE without 'FOR CHANNEL' clause.
START SLAVE;
ERROR HY000: Slave failed to initialize relay log info structure from the repository
=== Assert that all channels IO and SQL threads are in expected state ===
include/assert.inc [Corrupted channel's IO thread should be in OFF state.]
include/assert.inc [Corrupted channel's SQL thread should be in OFF state.]
===== ONLY one channel is corrupted. Waiting until started channels be 2.
===== ONLY one channel is corrupted. Waiting until started channels be 2.
===== SHOW RELAYLOG EVENTS without 'FOR CHANNEL' clause should throw error.
SHOW RELAYLOG EVENTS;
ERROR HY000: Multiple channels exist on the slave. Please provide channel name as an argument.
===== SELECT SOURCE_POS_WAIT(...) without 'FOR CHANNEL' argument should throw error.
SELECT SOURCE_POS_WAIT('server1-bin.000001', 120, 5);
ERROR HY000: Multiple channels exist on the slave. Please provide channel name as an argument.
===== SELECT WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS(...) without 'FOR CHANNEL' argument should throw error.
SELECT WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS('01010101-0101-0101-0101-010101010101:1', 1);
ERROR HY000: Multiple channels exist on the slave. Please provide channel name as an argument.
===== CHANGE MASTER without 'FOR CHANNEL' clause should throw error.
CHANGE REPLICATION SOURCE TO SOURCE_HEARTBEAT_PERIOD=10,  SOURCE_CONNECT_RETRY=10, SOURCE_RETRY_COUNT=10;
ERROR HY000: Multiple channels exist on the slave. Please provide channel name as an argument.
===== FLUSH RELAY LOGS without 'FOR CHANNEL' clause.
FLUSH RELAY LOGS;
===== Executing few basic commands on channel ''.
===== Executing SOURCE_POS_WAIT() and WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS().
[connection server_1]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return NULL for channel '' and position POSITION.]
[connection server_1]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return NULL for channel '' and position POSITION+1.]
===== Executing FLUSH RELAY LOGS FOR CHANNEL on channel ''.
FLUSH RELAY LOGS FOR CHANNEL '';
===== Executing SHOW RELAYLOG EVENTS FOR CHANNEL on channel ''.
SHOW RELAYLOG EVENTS FOR CHANNEL '';
===== Executing SHOW SLAVE FOR CHANNEL on channel ''.
Slave_IO_State = ''
Slave_SQL_Running_State = ''
Last_IO_Errno = '0'
Last_IO_Error = ''
Last_SQL_Errno = '13124'
Last_SQL_Error = 'Slave failed to initialize relay log info structure from the repository'
Channel_Name = ''
===== Executing STOP SLAVE FOR CHANNEL on channel ''.
include/stop_slave.inc [FOR CHANNEL '']
Warnings:
Note	3084	Replication thread(s) for channel '' are already stopped.
===== Executing CHANGE MASTER FOR CHANNEL on channel ''.
CHANGE REPLICATION SOURCE TO SOURCE_CONNECT_RETRY=100, SOURCE_RETRY_COUNT=100 FOR CHANNEL '';
Connect_Retry = '100'
Master_Retry_Count = '100'
Slave_IO_State = ''
Slave_SQL_Running_State = ''
Channel_Name = ''
===== Executing RESET SLAVE FOR CHANNEL on channel ''.
RESET SLAVE FOR CHANNEL '';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing RESET SLAVE ALL FOR CHANNEL on channel ''.
RESET SLAVE ALL FOR CHANNEL '';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing few basic commands on channel 'channel_2'.
===== Executing SOURCE_POS_WAIT() and WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS().
[connection server_2]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return 0 for channel 'channel_2' and position POSITION.]
[connection server_2]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return -1 for channel 'channel_2' and position POSITION+1.]
===== Executing FLUSH RELAY LOGS FOR CHANNEL on channel 'channel_2'.
FLUSH RELAY LOGS FOR CHANNEL 'channel_2';
===== Executing SHOW RELAYLOG EVENTS FOR CHANNEL on channel 'channel_2'.
SHOW RELAYLOG EVENTS FOR CHANNEL 'channel_2';
===== Executing SHOW SLAVE FOR CHANNEL on channel 'channel_2'.
include/wait_for_slave_param.inc [Slave_IO_State FOR CHANNEL 'channel_2']
include/wait_for_slave_param.inc [Slave_SQL_Running_State FOR CHANNEL 'channel_2']
Last_IO_Errno = '0'
Last_IO_Error = ''
Last_SQL_Errno = '0'
Last_SQL_Error = ''
Channel_Name = 'channel_2'
===== Check that replication is working fine on channel 'channel_2'.
[connection server_2]
CREATE TABLE t1(i INT);
INSERT INTO t1 VALUES (12);
DROP TABLE t1;
include/sync_slave_sql_with_master.inc [FOR CHANNEL 'channel_2']
===== Executing STOP SLAVE FOR CHANNEL on channel 'channel_2'.
include/stop_slave.inc [FOR CHANNEL 'channel_2']
===== Executing CHANGE MASTER FOR CHANNEL on channel 'channel_2'.
CHANGE REPLICATION SOURCE TO SOURCE_CONNECT_RETRY=100, SOURCE_RETRY_COUNT=100 FOR CHANNEL 'channel_2';
Connect_Retry = '100'
Master_Retry_Count = '100'
Slave_IO_State = ''
Slave_SQL_Running_State = ''
Channel_Name = 'channel_2'
===== Executing RESET SLAVE FOR CHANNEL on channel 'channel_2'.
RESET SLAVE FOR CHANNEL 'channel_2';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing RESET SLAVE ALL FOR CHANNEL on channel 'channel_2'.
RESET SLAVE ALL FOR CHANNEL 'channel_2';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing few basic commands on channel 'channel_3'.
===== Executing SOURCE_POS_WAIT() and WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS().
[connection server_3]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return 0 for channel 'channel_3' and position POSITION.]
[connection server_3]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return -1 for channel 'channel_3' and position POSITION+1.]
===== Executing FLUSH RELAY LOGS FOR CHANNEL on channel 'channel_3'.
FLUSH RELAY LOGS FOR CHANNEL 'channel_3';
===== Executing SHOW RELAYLOG EVENTS FOR CHANNEL on channel 'channel_3'.
SHOW RELAYLOG EVENTS FOR CHANNEL 'channel_3';
===== Executing SHOW SLAVE FOR CHANNEL on channel 'channel_3'.
include/wait_for_slave_param.inc [Slave_IO_State FOR CHANNEL 'channel_3']
include/wait_for_slave_param.inc [Slave_SQL_Running_State FOR CHANNEL 'channel_3']
Last_IO_Errno = '0'
Last_IO_Error = ''
Last_SQL_Errno = '0'
Last_SQL_Error = ''
Channel_Name = 'channel_3'
===== Check that replication is working fine on channel 'channel_3'.
[connection server_3]
CREATE TABLE t1(i INT);
INSERT INTO t1 VALUES (12);
DROP TABLE t1;
include/sync_slave_sql_with_master.inc [FOR CHANNEL 'channel_3']
===== Executing STOP SLAVE FOR CHANNEL on channel 'channel_3'.
include/stop_slave.inc [FOR CHANNEL 'channel_3']
===== Executing CHANGE MASTER FOR CHANNEL on channel 'channel_3'.
CHANGE REPLICATION SOURCE TO SOURCE_CONNECT_RETRY=100, SOURCE_RETRY_COUNT=100 FOR CHANNEL 'channel_3';
Connect_Retry = '100'
Master_Retry_Count = '100'
Slave_IO_State = ''
Slave_SQL_Running_State = ''
Channel_Name = 'channel_3'
===== Executing RESET SLAVE FOR CHANNEL on channel 'channel_3'.
RESET SLAVE FOR CHANNEL 'channel_3';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing RESET SLAVE ALL FOR CHANNEL on channel 'channel_3'.
RESET SLAVE ALL FOR CHANNEL 'channel_3';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
[connection server_1]
RESET MASTER;
[connection server_4]
CHANGE REPLICATION SOURCE TO SOURCE_USER='root', SOURCE_PORT= port, SOURCE_HOST='localhost' FOR CHANNEL '';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc [FOR CHANNEL '']
[connection server_1]
CREATE DATABASE db1;
CREATE TABLE db1.t1(i int);
DROP TABLE db1.t1;
DROP DATABASE db1;
include/sync_slave_sql_with_master.inc [FOR CHANNEL '']
[connection server_2]
RESET MASTER;
[connection server_4]
CHANGE REPLICATION SOURCE TO SOURCE_USER='root', SOURCE_PORT= port, SOURCE_HOST='localhost' FOR CHANNEL 'channel_2';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc [FOR CHANNEL 'channel_2']
[connection server_2]
CREATE DATABASE db2;
CREATE TABLE db2.t2(i int);
DROP TABLE db2.t2;
DROP DATABASE db2;
include/sync_slave_sql_with_master.inc [FOR CHANNEL 'channel_2']
[connection server_3]
RESET MASTER;
[connection server_4]
CHANGE REPLICATION SOURCE TO SOURCE_USER='root', SOURCE_PORT= port, SOURCE_HOST='localhost' FOR CHANNEL 'channel_3';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc [FOR CHANNEL 'channel_3']
[connection server_3]
CREATE DATABASE db3;
CREATE TABLE db3.t3(i int);
DROP TABLE db3.t3;
DROP DATABASE db3;
include/sync_slave_sql_with_master.inc [FOR CHANNEL 'channel_3']
===== Corrupting Channel 'channel_2'.
[connection server_4]
===== Stop the server 4.
include/rpl_stop_server.inc [server_number=4]
===== Remove 'channel_2' channel's relaylog file.
===== Start the server 4 with --skip-replica-start=0 option.
include/rpl_start_server.inc [server_number=4 parameters: --skip-replica-start=0]
===== Assert that all channels IO and SQL threads are in expected state.
include/assert.inc [Corrupted channel's IO thread should be in OFF state.]
include/assert.inc [Corrupted channel's SQL thread should be in OFF state.]
===== ONLY one channel is corrupted. Waiting until started channels be 2.
===== ONLY one channel is corrupted. Waiting until started channels be 2.
===== Executing few basic commands without 'FOR CHANNEL' clause.
===== STOP SLAVE without 'FOR CHANNEL' clause.
include/stop_slave.inc
Warnings:
Note	3084	Replication thread(s) for channel 'channel_2' are already stopped.
=== Assert that all channels IO and SQL threads are in OFF state ===
include/assert.inc [All the channel's IO thread should be in OFF state.]
include/assert.inc [All the channel's SQL thread should be in OFF state.]
===== START SLAVE without 'FOR CHANNEL' clause.
START SLAVE;
ERROR HY000: Slave failed to initialize relay log info structure from the repository
=== Assert that all channels IO and SQL threads are in expected state ===
include/assert.inc [Corrupted channel's IO thread should be in OFF state.]
include/assert.inc [Corrupted channel's SQL thread should be in OFF state.]
===== ONLY one channel is corrupted. Waiting until started channels be 2.
===== ONLY one channel is corrupted. Waiting until started channels be 2.
===== SHOW RELAYLOG EVENTS without 'FOR CHANNEL' clause should throw error.
SHOW RELAYLOG EVENTS;
ERROR HY000: Multiple channels exist on the slave. Please provide channel name as an argument.
===== SELECT SOURCE_POS_WAIT(...) without 'FOR CHANNEL' argument should throw error.
SELECT SOURCE_POS_WAIT('server1-bin.000001', 120, 5);
ERROR HY000: Multiple channels exist on the slave. Please provide channel name as an argument.
===== SELECT WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS(...) without 'FOR CHANNEL' argument should throw error.
SELECT WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS('01010101-0101-0101-0101-010101010101:1', 1);
ERROR HY000: Multiple channels exist on the slave. Please provide channel name as an argument.
===== CHANGE MASTER without 'FOR CHANNEL' clause should throw error.
CHANGE REPLICATION SOURCE TO SOURCE_HEARTBEAT_PERIOD=10,  SOURCE_CONNECT_RETRY=10, SOURCE_RETRY_COUNT=10;
ERROR HY000: Multiple channels exist on the slave. Please provide channel name as an argument.
===== FLUSH RELAY LOGS without 'FOR CHANNEL' clause.
FLUSH RELAY LOGS;
===== Executing few basic commands on channel ''.
===== Executing SOURCE_POS_WAIT() and WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS().
[connection server_1]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return 0 for channel '' and position POSITION.]
[connection server_1]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return -1 for channel '' and position POSITION+1.]
===== Executing FLUSH RELAY LOGS FOR CHANNEL on channel ''.
FLUSH RELAY LOGS FOR CHANNEL '';
===== Executing SHOW RELAYLOG EVENTS FOR CHANNEL on channel ''.
SHOW RELAYLOG EVENTS FOR CHANNEL '';
===== Executing SHOW SLAVE FOR CHANNEL on channel ''.
include/wait_for_slave_param.inc [Slave_IO_State FOR CHANNEL '']
include/wait_for_slave_param.inc [Slave_SQL_Running_State FOR CHANNEL '']
Last_IO_Errno = '0'
Last_IO_Error = ''
Last_SQL_Errno = '0'
Last_SQL_Error = ''
Channel_Name = ''
===== Check that replication is working fine on channel ''.
[connection server_1]
CREATE TABLE t1(i INT);
INSERT INTO t1 VALUES (12);
DROP TABLE t1;
include/sync_slave_sql_with_master.inc [FOR CHANNEL '']
===== Executing STOP SLAVE FOR CHANNEL on channel ''.
include/stop_slave.inc [FOR CHANNEL '']
===== Executing CHANGE MASTER FOR CHANNEL on channel ''.
CHANGE REPLICATION SOURCE TO SOURCE_CONNECT_RETRY=100, SOURCE_RETRY_COUNT=100 FOR CHANNEL '';
Connect_Retry = '100'
Master_Retry_Count = '100'
Slave_IO_State = ''
Slave_SQL_Running_State = ''
Channel_Name = ''
===== Executing RESET SLAVE FOR CHANNEL on channel ''.
RESET SLAVE FOR CHANNEL '';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing RESET SLAVE ALL FOR CHANNEL on channel ''.
RESET SLAVE ALL FOR CHANNEL '';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing few basic commands on channel 'channel_2'.
===== Executing SOURCE_POS_WAIT() and WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS().
[connection server_2]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return NULL for channel 'channel_2' and position POSITION.]
[connection server_2]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return NULL for channel 'channel_2' and position POSITION+1.]
===== Executing FLUSH RELAY LOGS FOR CHANNEL on channel 'channel_2'.
FLUSH RELAY LOGS FOR CHANNEL 'channel_2';
===== Executing SHOW RELAYLOG EVENTS FOR CHANNEL on channel 'channel_2'.
SHOW RELAYLOG EVENTS FOR CHANNEL 'channel_2';
===== Executing SHOW SLAVE FOR CHANNEL on channel 'channel_2'.
Slave_IO_State = ''
Slave_SQL_Running_State = ''
Last_IO_Errno = '0'
Last_IO_Error = ''
Last_SQL_Errno = '13124'
Last_SQL_Error = 'Slave failed to initialize relay log info structure from the repository'
Channel_Name = 'channel_2'
===== Executing STOP SLAVE FOR CHANNEL on channel 'channel_2'.
include/stop_slave.inc [FOR CHANNEL 'channel_2']
Warnings:
Note	3084	Replication thread(s) for channel 'channel_2' are already stopped.
===== Executing CHANGE MASTER FOR CHANNEL on channel 'channel_2'.
CHANGE REPLICATION SOURCE TO SOURCE_CONNECT_RETRY=100, SOURCE_RETRY_COUNT=100 FOR CHANNEL 'channel_2';
Connect_Retry = '100'
Master_Retry_Count = '100'
Slave_IO_State = ''
Slave_SQL_Running_State = ''
Channel_Name = 'channel_2'
===== Executing RESET SLAVE FOR CHANNEL on channel 'channel_2'.
RESET SLAVE FOR CHANNEL 'channel_2';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing RESET SLAVE ALL FOR CHANNEL on channel 'channel_2'.
RESET SLAVE ALL FOR CHANNEL 'channel_2';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing few basic commands on channel 'channel_3'.
===== Executing SOURCE_POS_WAIT() and WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS().
[connection server_3]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return 0 for channel 'channel_3' and position POSITION.]
[connection server_3]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return -1 for channel 'channel_3' and position POSITION+1.]
===== Executing FLUSH RELAY LOGS FOR CHANNEL on channel 'channel_3'.
FLUSH RELAY LOGS FOR CHANNEL 'channel_3';
===== Executing SHOW RELAYLOG EVENTS FOR CHANNEL on channel 'channel_3'.
SHOW RELAYLOG EVENTS FOR CHANNEL 'channel_3';
===== Executing SHOW SLAVE FOR CHANNEL on channel 'channel_3'.
include/wait_for_slave_param.inc [Slave_IO_State FOR CHANNEL 'channel_3']
include/wait_for_slave_param.inc [Slave_SQL_Running_State FOR CHANNEL 'channel_3']
Last_IO_Errno = '0'
Last_IO_Error = ''
Last_SQL_Errno = '0'
Last_SQL_Error = ''
Channel_Name = 'channel_3'
===== Check that replication is working fine on channel 'channel_3'.
[connection server_3]
CREATE TABLE t1(i INT);
INSERT INTO t1 VALUES (12);
DROP TABLE t1;
include/sync_slave_sql_with_master.inc [FOR CHANNEL 'channel_3']
===== Executing STOP SLAVE FOR CHANNEL on channel 'channel_3'.
include/stop_slave.inc [FOR CHANNEL 'channel_3']
===== Executing CHANGE MASTER FOR CHANNEL on channel 'channel_3'.
CHANGE REPLICATION SOURCE TO SOURCE_CONNECT_RETRY=100, SOURCE_RETRY_COUNT=100 FOR CHANNEL 'channel_3';
Connect_Retry = '100'
Master_Retry_Count = '100'
Slave_IO_State = ''
Slave_SQL_Running_State = ''
Channel_Name = 'channel_3'
===== Executing RESET SLAVE FOR CHANNEL on channel 'channel_3'.
RESET SLAVE FOR CHANNEL 'channel_3';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing RESET SLAVE ALL FOR CHANNEL on channel 'channel_3'.
RESET SLAVE ALL FOR CHANNEL 'channel_3';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
[connection server_1]
RESET MASTER;
[connection server_4]
CHANGE REPLICATION SOURCE TO SOURCE_USER='root', SOURCE_PORT= port, SOURCE_HOST='localhost' FOR CHANNEL '';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc [FOR CHANNEL '']
[connection server_1]
CREATE DATABASE db1;
CREATE TABLE db1.t1(i int);
DROP TABLE db1.t1;
DROP DATABASE db1;
include/sync_slave_sql_with_master.inc [FOR CHANNEL '']
[connection server_2]
RESET MASTER;
[connection server_4]
CHANGE REPLICATION SOURCE TO SOURCE_USER='root', SOURCE_PORT= port, SOURCE_HOST='localhost' FOR CHANNEL 'channel_2';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc [FOR CHANNEL 'channel_2']
[connection server_2]
CREATE DATABASE db2;
CREATE TABLE db2.t2(i int);
DROP TABLE db2.t2;
DROP DATABASE db2;
include/sync_slave_sql_with_master.inc [FOR CHANNEL 'channel_2']
[connection server_3]
RESET MASTER;
[connection server_4]
CHANGE REPLICATION SOURCE TO SOURCE_USER='root', SOURCE_PORT= port, SOURCE_HOST='localhost' FOR CHANNEL 'channel_3';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc [FOR CHANNEL 'channel_3']
[connection server_3]
CREATE DATABASE db3;
CREATE TABLE db3.t3(i int);
DROP TABLE db3.t3;
DROP DATABASE db3;
include/sync_slave_sql_with_master.inc [FOR CHANNEL 'channel_3']
===== Corrupting Channel 'channel_3'.
[connection server_4]
===== Stop the server 4.
include/rpl_stop_server.inc [server_number=4]
===== Remove 'channel_3' channel's relaylog file.
===== Start the server 4 with --skip-replica-start=0 option.
include/rpl_start_server.inc [server_number=4 parameters: --skip-replica-start=0]
===== Assert that all channels IO and SQL threads are in expected state.
include/assert.inc [Corrupted channel's IO thread should be in OFF state.]
include/assert.inc [Corrupted channel's SQL thread should be in OFF state.]
===== ONLY one channel is corrupted. Waiting until started channels be 2.
===== ONLY one channel is corrupted. Waiting until started channels be 2.
===== Executing few basic commands without 'FOR CHANNEL' clause.
===== STOP SLAVE without 'FOR CHANNEL' clause.
include/stop_slave.inc
Warnings:
Note	3084	Replication thread(s) for channel 'channel_3' are already stopped.
=== Assert that all channels IO and SQL threads are in OFF state ===
include/assert.inc [All the channel's IO thread should be in OFF state.]
include/assert.inc [All the channel's SQL thread should be in OFF state.]
===== START SLAVE without 'FOR CHANNEL' clause.
START SLAVE;
ERROR HY000: Slave failed to initialize relay log info structure from the repository
=== Assert that all channels IO and SQL threads are in expected state ===
include/assert.inc [Corrupted channel's IO thread should be in OFF state.]
include/assert.inc [Corrupted channel's SQL thread should be in OFF state.]
===== ONLY one channel is corrupted. Waiting until started channels be 2.
===== ONLY one channel is corrupted. Waiting until started channels be 2.
===== SHOW RELAYLOG EVENTS without 'FOR CHANNEL' clause should throw error.
SHOW RELAYLOG EVENTS;
ERROR HY000: Multiple channels exist on the slave. Please provide channel name as an argument.
===== SELECT SOURCE_POS_WAIT(...) without 'FOR CHANNEL' argument should throw error.
SELECT SOURCE_POS_WAIT('server1-bin.000001', 120, 5);
ERROR HY000: Multiple channels exist on the slave. Please provide channel name as an argument.
===== SELECT WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS(...) without 'FOR CHANNEL' argument should throw error.
SELECT WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS('01010101-0101-0101-0101-010101010101:1', 1);
ERROR HY000: Multiple channels exist on the slave. Please provide channel name as an argument.
===== CHANGE MASTER without 'FOR CHANNEL' clause should throw error.
CHANGE REPLICATION SOURCE TO SOURCE_HEARTBEAT_PERIOD=10,  SOURCE_CONNECT_RETRY=10, SOURCE_RETRY_COUNT=10;
ERROR HY000: Multiple channels exist on the slave. Please provide channel name as an argument.
===== FLUSH RELAY LOGS without 'FOR CHANNEL' clause.
FLUSH RELAY LOGS;
===== Executing few basic commands on channel ''.
===== Executing SOURCE_POS_WAIT() and WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS().
[connection server_1]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return 0 for channel '' and position POSITION.]
[connection server_1]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return -1 for channel '' and position POSITION+1.]
===== Executing FLUSH RELAY LOGS FOR CHANNEL on channel ''.
FLUSH RELAY LOGS FOR CHANNEL '';
===== Executing SHOW RELAYLOG EVENTS FOR CHANNEL on channel ''.
SHOW RELAYLOG EVENTS FOR CHANNEL '';
===== Executing SHOW SLAVE FOR CHANNEL on channel ''.
include/wait_for_slave_param.inc [Slave_IO_State FOR CHANNEL '']
include/wait_for_slave_param.inc [Slave_SQL_Running_State FOR CHANNEL '']
Last_IO_Errno = '0'
Last_IO_Error = ''
Last_SQL_Errno = '0'
Last_SQL_Error = ''
Channel_Name = ''
===== Check that replication is working fine on channel ''.
[connection server_1]
CREATE TABLE t1(i INT);
INSERT INTO t1 VALUES (12);
DROP TABLE t1;
include/sync_slave_sql_with_master.inc [FOR CHANNEL '']
===== Executing STOP SLAVE FOR CHANNEL on channel ''.
include/stop_slave.inc [FOR CHANNEL '']
===== Executing CHANGE MASTER FOR CHANNEL on channel ''.
CHANGE REPLICATION SOURCE TO SOURCE_CONNECT_RETRY=100, SOURCE_RETRY_COUNT=100 FOR CHANNEL '';
Connect_Retry = '100'
Master_Retry_Count = '100'
Slave_IO_State = ''
Slave_SQL_Running_State = ''
Channel_Name = ''
===== Executing RESET SLAVE FOR CHANNEL on channel ''.
RESET SLAVE FOR CHANNEL '';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing RESET SLAVE ALL FOR CHANNEL on channel ''.
RESET SLAVE ALL FOR CHANNEL '';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing few basic commands on channel 'channel_2'.
===== Executing SOURCE_POS_WAIT() and WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS().
[connection server_2]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return 0 for channel 'channel_2' and position POSITION.]
[connection server_2]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return -1 for channel 'channel_2' and position POSITION+1.]
===== Executing FLUSH RELAY LOGS FOR CHANNEL on channel 'channel_2'.
FLUSH RELAY LOGS FOR CHANNEL 'channel_2';
===== Executing SHOW RELAYLOG EVENTS FOR CHANNEL on channel 'channel_2'.
SHOW RELAYLOG EVENTS FOR CHANNEL 'channel_2';
===== Executing SHOW SLAVE FOR CHANNEL on channel 'channel_2'.
include/wait_for_slave_param.inc [Slave_IO_State FOR CHANNEL 'channel_2']
include/wait_for_slave_param.inc [Slave_SQL_Running_State FOR CHANNEL 'channel_2']
Last_IO_Errno = '0'
Last_IO_Error = ''
Last_SQL_Errno = '0'
Last_SQL_Error = ''
Channel_Name = 'channel_2'
===== Check that replication is working fine on channel 'channel_2'.
[connection server_2]
CREATE TABLE t1(i INT);
INSERT INTO t1 VALUES (12);
DROP TABLE t1;
include/sync_slave_sql_with_master.inc [FOR CHANNEL 'channel_2']
===== Executing STOP SLAVE FOR CHANNEL on channel 'channel_2'.
include/stop_slave.inc [FOR CHANNEL 'channel_2']
===== Executing CHANGE MASTER FOR CHANNEL on channel 'channel_2'.
CHANGE REPLICATION SOURCE TO SOURCE_CONNECT_RETRY=100, SOURCE_RETRY_COUNT=100 FOR CHANNEL 'channel_2';
Connect_Retry = '100'
Master_Retry_Count = '100'
Slave_IO_State = ''
Slave_SQL_Running_State = ''
Channel_Name = 'channel_2'
===== Executing RESET SLAVE FOR CHANNEL on channel 'channel_2'.
RESET SLAVE FOR CHANNEL 'channel_2';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing RESET SLAVE ALL FOR CHANNEL on channel 'channel_2'.
RESET SLAVE ALL FOR CHANNEL 'channel_2';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing few basic commands on channel 'channel_3'.
===== Executing SOURCE_POS_WAIT() and WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS().
[connection server_3]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return NULL for channel 'channel_3' and position POSITION.]
[connection server_3]
[connection server_4]
include/assert.inc [SOURCE_POS_WAIT should return NULL for channel 'channel_3' and position POSITION+1.]
===== Executing FLUSH RELAY LOGS FOR CHANNEL on channel 'channel_3'.
FLUSH RELAY LOGS FOR CHANNEL 'channel_3';
===== Executing SHOW RELAYLOG EVENTS FOR CHANNEL on channel 'channel_3'.
SHOW RELAYLOG EVENTS FOR CHANNEL 'channel_3';
===== Executing SHOW SLAVE FOR CHANNEL on channel 'channel_3'.
Slave_IO_State = ''
Slave_SQL_Running_State = ''
Last_IO_Errno = '0'
Last_IO_Error = ''
Last_SQL_Errno = '13124'
Last_SQL_Error = 'Slave failed to initialize relay log info structure from the repository'
Channel_Name = 'channel_3'
===== Executing STOP SLAVE FOR CHANNEL on channel 'channel_3'.
include/stop_slave.inc [FOR CHANNEL 'channel_3']
Warnings:
Note	3084	Replication thread(s) for channel 'channel_3' are already stopped.
===== Executing CHANGE MASTER FOR CHANNEL on channel 'channel_3'.
CHANGE REPLICATION SOURCE TO SOURCE_CONNECT_RETRY=100, SOURCE_RETRY_COUNT=100 FOR CHANNEL 'channel_3';
Connect_Retry = '100'
Master_Retry_Count = '100'
Slave_IO_State = ''
Slave_SQL_Running_State = ''
Channel_Name = 'channel_3'
===== Executing RESET SLAVE FOR CHANNEL on channel 'channel_3'.
RESET SLAVE FOR CHANNEL 'channel_3';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
===== Executing RESET SLAVE ALL FOR CHANNEL on channel 'channel_3'.
RESET SLAVE ALL FOR CHANNEL 'channel_3';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/rpl_end.inc
