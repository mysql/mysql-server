include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/assert.inc [On master, the table should return an empty set.]
include/assert.inc [On slave, the group_name should be set NULL]

# Verify that SELECT works for every field and produces an output similar
# to the corresponding field in SHOW SLAVE STATUS(SSS). To verify the
# correctness of thread_id field, we check for the name of the thread.

include/assert.inc [Value returned by SSS and PS table for Source_UUID should be same.]
include/assert.inc [thread_name should should indicate io thread.]
include/assert.inc [SSS shows Slave_IO_Running as "Yes". So, Service_State from this PS table should be "ON".]
include/assert.inc [PS table should output 0000-00-00 00:00:00.000000 indicating no heartbeats have been received]
include/assert.inc [Value returned by SSS and PS table for Received_Transaction_Set should be same.]
include/assert.inc [Value returned by SSS and PS table for Last_Error_Number should be same.]
include/assert.inc [Value returned by SSS and PS table for Last_Error_Message should be same.]
include/assert.inc [Value returned by PS table for Last_Error_Timestamp should be 0000-00-00 00:00:00.000000.]
include/stop_slave.inc

# heartbeat count and last heartbeat timestamp are indeterministic so we
# can not test for their exact values. We will thus check for >0 number
# of heartbeats and last heartbeat timestamp!= zeros.

CHANGE REPLICATION SOURCE to SOURCE_HEARTBEAT_PERIOD= 0.5;
set @restore_replica_net_timeout= @@global.replica_net_timeout;
set @@global.replica_net_timeout= 10;
include/start_slave.inc

A heartbeat has been received by the slave

include/assert.inc [last_heartbeat_timestamp should NOT be zero]
set @@global.replica_net_timeout= @restore_replica_net_timeout;

# We now introduce an error in the IO thread and check for the correctness
# of error number, message and timestamp fields.

include/stop_slave.inc
CHANGE REPLICATION SOURCE to
SOURCE_USER='replssl',
SOURCE_PASSWORD='';
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
start slave io_thread;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_param.inc [Last_IO_Errno]

# Extract the error related fields from SSS and PS table and compare
# them for correctness.

include/assert.inc [Value returned by SSS and PS table for Last_Error_Number should be same.]
Value returned by SSS and PS table for Last_Error_Message is same.
include/assert.inc [Value returned by SSS and PS table for Last_Error_Timestamp should be same.]

# Verify that source_UUID and the error related fields are preserved
# after STOP SLAVE, thread_id changes to NULL and service_state to "off".

include/stop_slave.inc

# 1. Verify that thread_id changes to NULL and service_state to "off" on
#    STOP SLAVE.

include/assert.inc [After STOP SLAVE, thread_id should be NULL]
include/assert.inc [SSS shows Slave_IO_Running as "No". So, Service_State from this PS table should be "OFF".]

# 2. Extract the source_UUID and the error related fields from SSS and PS
#    table and compare them. These fields should preserve their values.

include/assert.inc [Value returned by SSS and PS table for Source_UUID should be same.]
include/assert.inc [Value returned by SSS and PS table for Last_Error_Number should be same.]
Value returned by SSS and PS table for Last_Error_Message is same.
include/assert.inc [Value returned by SSS and PS table for Last_Error_Timestamp should be same.]

# Restart the master and slave servers in gtid-mode=on, execute some
# transactions and verify the 'Received_Transaction_Set' field.

include/rpl_restart_server.inc [server_number=1 gtids=on]
include/rpl_restart_server.inc [server_number=2 gtids=on]
CHANGE REPLICATION SOURCE to
SOURCE_USER = 'root',
SOURCE_AUTO_POSITION= 1;
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc
use test;
create table t(a int);
insert into t values(1);
include/sync_slave_sql_with_master.inc
include/assert.inc [Value returned by SSS and PS table for Received_Transaction_Set should be same.]

# Verify that the value for 'Received_Transaction_Set' field is preserved
# after STOP SLAVE.

include/stop_slave.inc
include/assert.inc [Received_Transaction_Set should not have changed after STOP SLAVE]

# Verify that the value for 'Received_Transaction_Set' is cleared
# after RESET SLAVE.

RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/assert.inc [Received_Transaction_Set should be empty after RESET SLAVE]
include/start_slave.inc
drop table t;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION = 0 FOR CHANNEL '';
include/rpl_set_gtid_mode.inc
include/rpl_end.inc
