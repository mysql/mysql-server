include/master-slave.inc
[connection master]
[connection slave]
set @restore_replica_net_timeout= @@global.replica_net_timeout;
set @@global.replica_net_timeout= 10;
SELECT LAST_HEARTBEAT_TIMESTAMP FROM performance_schema.replication_connection_status;
CHANGE REPLICATION SOURCE to SOURCE_HOST='127.0.0.1',SOURCE_PORT=SOURCE_PORT, SOURCE_USER='root';
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
SELECT HEARTBEAT_INTERVAL FROM performance_schema.replication_connection_configuration;
HEARTBEAT_INTERVAL	5.000
CHANGE REPLICATION SOURCE to SOURCE_HOST='127.0.0.1',SOURCE_PORT=SOURCE_PORT, SOURCE_USER='root', SOURCE_HEARTBEAT_PERIOD= 4294968;
ERROR HY000: The requested value for the heartbeat period is either negative or exceeds the maximum allowed (4294967 seconds).
SELECT HEARTBEAT_INTERVAL FROM performance_schema.replication_connection_configuration;
HEARTBEAT_INTERVAL	5.000
[connection slave]
CHANGE REPLICATION SOURCE to SOURCE_HOST='127.0.0.1',SOURCE_PORT=SOURCE_PORT, SOURCE_USER='root', SOURCE_HEARTBEAT_PERIOD= 0.0009999;
Warnings:
Warning	####	The requested value for the heartbeat period is less than 1 millisecond. The value is reset to 0, meaning that heartbeating will effectively be disabled.
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
SELECT HEARTBEAT_INTERVAL FROM performance_schema.replication_connection_configuration;
HEARTBEAT_INTERVAL	0.000
CHANGE REPLICATION SOURCE to SOURCE_HOST='127.0.0.1',SOURCE_PORT=SOURCE_PORT, SOURCE_USER='root', SOURCE_HEARTBEAT_PERIOD= 4294967;
Warnings:
Warning	####	The requested value for the heartbeat period exceeds the value of `replica_net_timeout' seconds. A sensible value for the period should be less than the timeout.
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
SELECT HEARTBEAT_INTERVAL FROM performance_schema.replication_connection_configuration;
HEARTBEAT_INTERVAL	4294967.000
CHANGE REPLICATION SOURCE to SOURCE_HOST='127.0.0.1',SOURCE_PORT=SOURCE_PORT, SOURCE_USER='root', SOURCE_HEARTBEAT_PERIOD= 0.001;
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
SELECT HEARTBEAT_INTERVAL FROM performance_schema.replication_connection_configuration;
HEARTBEAT_INTERVAL	0.001
reset slave;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
set @@global.replica_net_timeout= 5;
CHANGE REPLICATION SOURCE to SOURCE_HOST='127.0.0.1',SOURCE_PORT=SOURCE_PORT, SOURCE_USER='root', SOURCE_HEARTBEAT_PERIOD= 5.001;
Warnings:
Warning	####	The requested value for the heartbeat period exceeds the value of `replica_net_timeout' seconds. A sensible value for the period should be less than the timeout.
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
SELECT HEARTBEAT_INTERVAL FROM performance_schema.replication_connection_configuration;
HEARTBEAT_INTERVAL	5.001
reset slave;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
set @@global.replica_net_timeout= 5;
Warnings:
Warning	1704	The requested value for the heartbeat period exceeds the value of `replica_net_timeout' seconds. A sensible value for the period should be less than the timeout.
CHANGE REPLICATION SOURCE to SOURCE_HOST='127.0.0.1',SOURCE_PORT=SOURCE_PORT, SOURCE_USER='root', SOURCE_HEARTBEAT_PERIOD= 4;
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
SELECT HEARTBEAT_INTERVAL FROM performance_schema.replication_connection_configuration;
HEARTBEAT_INTERVAL	4.000
set @@global.replica_net_timeout= 3 /* must be a warning */;
Warnings:
Warning	1704	The requested value for the heartbeat period exceeds the value of `replica_net_timeout' seconds. A sensible value for the period should be less than the timeout.
reset slave;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
[connection master]
drop table if exists t1;
[connection slave]
set @@global.replica_net_timeout= 10;
CHANGE REPLICATION SOURCE to SOURCE_HOST='127.0.0.1',SOURCE_PORT=SOURCE_PORT, SOURCE_USER='root', SOURCE_HEARTBEAT_PERIOD= 0.5;
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
SELECT HEARTBEAT_INTERVAL FROM performance_schema.replication_connection_configuration;
HEARTBEAT_INTERVAL	0.500
start slave;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
[connection master]
create table t1 (f1 int);
include/sync_slave_sql_with_master.inc
include/check_slave_param.inc [Relay_Log_File]
SELECT HEARTBEAT_INTERVAL FROM performance_schema.replication_connection_configuration;
HEARTBEAT_INTERVAL	0.500
A heartbeat has been received by the slave
include/assert.inc [last_heartbeat_timestamp should not be empty]
[connection master]
drop table t1;
include/sync_slave_sql_with_master.inc
set @@global.replica_net_timeout= @restore_replica_net_timeout;
[connection slave]
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_HEARTBEAT_PERIOD= 10;
include/assert.inc [HEARTBEAT_INTERVAL should be 10.000]

# Verify that heartbeat period is not reset to default implicitly and
# received_heartbeat is not set to 0 by this.

CHANGE REPLICATION SOURCE TO SOURCE_RETRY_COUNT= 3;
include/assert.inc [HEARTBEAT_INTERVAL should be 10.000]
include/assert.inc [COUNT_RECEIVED_HEARTBEATS should NOT be set to 0]
include/stop_slave.inc
Warnings:
Note	3084	Replication thread(s) for channel '' are already stopped.
RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
RESET MASTER;
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION;
include/start_slave.inc
include/assert.inc [HEARTBEAT_INTERVAL should still be 10.000 after RESET SLAVE.]
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_HOST= 'localhost', SOURCE_PORT= SOURCE_PORT;
include/start_slave.inc
include/assert.inc [HEARTBEAT_INTERVAL should reset to default.]
#
# Change from default to make sure changing HOST resets heartbeat_interval
# to default.
#
include/stop_slave_io.inc
CHANGE REPLICATION SOURCE TO SOURCE_HEARTBEAT_PERIOD = 10;
include/start_slave.inc
DROP TABLE IF EXISTS t1;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_HOST= '127.0.0.1';
include/start_slave.inc
include/assert.inc [heartbeat_interval should reset to default on changing host.]
DROP TABLE IF EXISTS t1;
include/sync_slave_sql_with_master.inc
#
# Change from default to make sure changing PORT resets heartbeat_interval
# to default.
#
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_HEARTBEAT_PERIOD = 10;
include/start_slave.inc
DROP TABLE IF EXISTS t1;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_PORT= SOURCE_PORT;
include/start_slave.inc
include/assert.inc [heartbeat_interval should reset to default on changing port.]
DROP TABLE IF EXISTS t1;
include/sync_slave_sql_with_master.inc
#
# Change from default to make sure changing HOST and PORT resets
# heartbeat_period to default.
#
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_HOST= '127.0.0.1', SOURCE_PORT= SOURCE_PORT;
include/start_slave.inc
include/assert.inc [heartbeat_interval should reset to default on changing host and port.]
DROP TABLE IF EXISTS t1;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [heartbeat_interval should reset to zero by RESET SLAVE ALL.]
CHANGE REPLICATION SOURCE to SOURCE_HOST= '127.0.0.1', SOURCE_PORT= SOURCE_PORT, SOURCE_USER= 'root';
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc
DROP TABLE IF EXISTS t1;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
