include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
call mtr.add_suppression("Detected misconfiguration: replication channel '' was configured with SOURCE_CONNECTION_AUTO_FAILOVER = 1.*");
#
# 1. Enable AUTO_POSITION.
#
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION=1;
#
# 2. Verify disabling AUTO_POSITION while enabling Asynchronous
#    Replication Connection Failover results in
#    ER_RPL_ASYNC_RECONNECT_AUTO_POSITION_OFF
#
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION=0, SOURCE_CONNECTION_AUTO_FAILOVER=1;
ERROR HY000: Failed to enable Asynchronous Replication Connection Failover feature. The CHANGE MASTER TO SOURCE_CONNECTION_AUTO_FAILOVER = 1 can only be set when MASTER_AUTO_POSITION option of CHANGE MASTER TO is enabled.
#
# 3. Verify Asynchronous Replication Connection Failover is disabled and
#    Auto_position is enabled.
#
include/assert.inc ['Asynchronous Replication Connection Failover is disabled']
include/assert.inc ['Auto_position is enabled']
#
# 4. Verify restart server with Asynchronous Replication Connection
#    Failover enabled and GTID_MODE = OFF_PERMISSIVE will stop replica
#    thread from start with ER_RPL_ASYNC_RECONNECT_GTID_MODE_OFF_CHANNEL
#    error logged in error log.
#
CHANGE REPLICATION SOURCE TO SOURCE_CONNECTION_AUTO_FAILOVER=1;
include/rpl_restart_server.inc [server_number=2 parameters: --gtid-mode=off_permissive]
include/assert_grep.inc [Replication channel '' could not be started as GTID_MODE was not ON.]
include/check_slave_param.inc [Slave_IO_Running FOR CHANNEL '']
#
# 5. Verify start replica after restart server also does not start
#    replica threads and fails with with
#    ER_RPL_ASYNC_RECONNECT_GTID_MODE_OFF error.
#
START SLAVE;
ERROR HY000: Failed to enable Asynchronous Replication Connection Failover feature. The CHANGE MASTER TO SOURCE_CONNECTION_AUTO_FAILOVER = 1 can only be set when @@GLOBAL.GTID_MODE = ON.
#
# 6. Clean up.
#
CHANGE REPLICATION SOURCE TO SOURCE_CONNECTION_AUTO_FAILOVER = 0;
SET @@global.gtid_mode = ON_PERMISSIVE;
SET @@global.gtid_mode = ON;
include/rpl_end.inc
