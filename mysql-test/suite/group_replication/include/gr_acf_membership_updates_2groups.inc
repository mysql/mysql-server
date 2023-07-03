#
# Validate that list of senders is updated when a replica has two configured
# groups
#
# Test:
#   0. The test requires 5 servers:
#        server1: group A primary
#        server2: group A secondary
#        server3: group B primary
#        server4: group B secondary
#        server5: single server
#   1. Deploy a 1 member group A in single primary mode.
#   2. Deploy a 1 member group B in single primary mode.
#   3. Configure replica to manage the asynchronous connection
#      to groups A and B.
#   4. Create and start the managed channel.
#   5. Add secondary member to group A and validate is added to list of senders
#   6. Add secondary member to group B and validate is added to list of senders
#   7. Remove secondary member from group B and validate is removed from
#      list of senders
#   8. Remove secondary member from group A and validate is removed from
#      list of senders
#   9. Clean up.
#
################################################################################

--source include/big_test.inc
--source include/have_group_replication_plugin.inc
--let $rpl_skip_group_replication_start= 1
--let $rpl_server_count= 5
--source include/group_replication.inc

--echo
--echo # 1. Deploy a 1 member group A in single primary mode.
--echo
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
--let $server1_uuid= `SELECT @@server_uuid`
--let $group_replication_group_name= aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa
--let $group_a= $group_replication_group_name
--disable_query_log
--eval SET GLOBAL group_replication_group_seeds= "localhost:$SERVER_GR_PORT_1,localhost:$SERVER_GR_PORT_2"
--enable_query_log
--source include/start_and_bootstrap_group_replication.inc

--echo
--echo # 2. Deploy a 1 member group B in single primary mode.
--echo
--let $rpl_connection_name= server3
--source include/rpl_connection.inc
--let $server3_uuid= `SELECT @@server_uuid`
--let $group_replication_group_name= bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb
--let $group_b= $group_replication_group_name
--disable_query_log
if ($have_xcom_stack){
--eval SET GLOBAL group_replication_group_seeds= "localhost:$SERVER_GR_PORT_3,localhost:$SERVER_GR_PORT_4"
}
if (!$have_xcom_stack){
--eval SET GLOBAL group_replication_group_seeds= "localhost:$SERVER_MYPORT_3,localhost:$SERVER_MYPORT_4"
}
--enable_query_log
--source include/start_and_bootstrap_group_replication.inc

--echo
--echo # 3. Configure replica to manage the asynchronous connection
--echo #    to groups A and B.
--echo
--let $rpl_connection_name= server5
--source include/rpl_connection.inc

--replace_result $SERVER_MYPORT_1 SERVER_MYPORT_1 $group_a GROUP_A
--eval SELECT asynchronous_connection_failover_add_managed('ch1', 'GroupReplication', '$group_a', '127.0.0.1', $SERVER_MYPORT_1, '', 90, 70);

--replace_result $SERVER_MYPORT_3 SERVER_MYPORT_3 $group_b GROUP_B
--eval SELECT asynchronous_connection_failover_add_managed('ch1', 'GroupReplication', '$group_b', '127.0.0.1', $SERVER_MYPORT_3, '', 85, 65);

--let $assert_text= 'There is one row in performance_schema.replication_asynchronous_connection_failover for server1'
--let $assert_cond= [SELECT  COUNT(*) count FROM performance_schema.replication_asynchronous_connection_failover WHERE port=$SERVER_MYPORT_1, count, 1] = 1
--source include/assert.inc

--let $assert_text= 'There is one row in performance_schema.replication_asynchronous_connection_failover for server3'
--let $assert_cond= [SELECT  COUNT(*) count FROM performance_schema.replication_asynchronous_connection_failover WHERE port=$SERVER_MYPORT_3, count, 1] = 1
--source include/assert.inc

--let $assert_text= 'There is one row in performance_schema.replication_asynchronous_connection_failover_managed for group A'
--let $assert_cond= [SELECT  COUNT(*) count FROM performance_schema.replication_asynchronous_connection_failover_managed WHERE managed_name="$group_a", count, 1] = 1
--source include/assert.inc

--let $assert_text= 'There is one row in performance_schema.replication_asynchronous_connection_failover_managed for group B'
--let $assert_cond= [SELECT  COUNT(*) count FROM performance_schema.replication_asynchronous_connection_failover_managed WHERE managed_name="$group_b", count, 1] = 1
--source include/assert.inc

--echo
--echo # 4. Create and start the managed channel.
--echo
--let $rpl_connection_name= server5
--source include/rpl_connection.inc
--replace_result $SERVER_MYPORT_1 SERVER_1_PORT
--eval CHANGE REPLICATION SOURCE TO SOURCE_HOST='127.0.0.1', SOURCE_USER='root', SOURCE_AUTO_POSITION=1, SOURCE_CONNECTION_AUTO_FAILOVER=1, SOURCE_PORT=$SERVER_MYPORT_1, SOURCE_CONNECT_RETRY=1, SOURCE_RETRY_COUNT=1 FOR CHANNEL 'ch1'

--let $rpl_channel_name='ch1'
--source include/start_slave.inc
--let $rpl_channel_name=

--let $assert_text= Verify channel ch1 IO_THREAD is ON and connected to server1
--let $assert_cond= "[SELECT SERVICE_STATE FROM performance_schema.replication_connection_status WHERE channel_name=\'ch1\' AND source_uuid=\'$server1_uuid\', SERVICE_STATE, 1]" = "ON"
--source include/assert.inc

--let $wait_condition= SELECT COUNT(*)=2 FROM performance_schema.replication_asynchronous_connection_failover
--source include/wait_condition.inc

--echo
--echo # 5. Add secondary member to group A and validate is added to list of senders
--echo
--let $rpl_connection_name= server2
--source include/rpl_connection.inc
--let $server2_uuid= `SELECT @@server_uuid`
--let $group_replication_group_name= $group_a
--disable_query_log
if ($have_xcom_stack){
--eval SET GLOBAL group_replication_group_seeds= "localhost:$SERVER_GR_PORT_1,localhost:$SERVER_GR_PORT_2"
}

if (!$have_xcom_stack){
--eval SET GLOBAL group_replication_group_seeds= "localhost:$SERVER_MYPORT_1,localhost:$SERVER_MYPORT_2"
}
--enable_query_log
--source include/start_group_replication.inc

--let $rpl_connection_name= server5
--source include/rpl_connection.inc
--let $wait_condition= SELECT COUNT(*)=3 FROM performance_schema.replication_asynchronous_connection_failover
--source include/wait_condition.inc

--echo
--echo # 6. Add secondary member to group B and validate is added to list of senders
--echo
--let $rpl_connection_name= server4
--source include/rpl_connection.inc
--let $server4_uuid= `SELECT @@server_uuid`
--let $group_replication_group_name= $group_b
--disable_query_log
if ($have_xcom_stack){
--eval SET GLOBAL group_replication_group_seeds= "localhost:$SERVER_GR_PORT_3,localhost:$SERVER_GR_PORT_4"
}
if (!$have_xcom_stack){
--eval SET GLOBAL group_replication_group_seeds= "localhost:$SERVER_MYPORT_3,localhost:$SERVER_MYPORT_4"
}
--enable_query_log
--source include/start_group_replication.inc

--let $rpl_connection_name= server5
--source include/rpl_connection.inc
--let $wait_condition= SELECT COUNT(*)=4 FROM performance_schema.replication_asynchronous_connection_failover
--source include/wait_condition.inc

--echo
--echo # 7. Remove secondary member from group B and validate is removed from
--echo #    list of senders
--echo
--let $rpl_connection_name= server4
--source include/rpl_connection.inc
--source include/stop_group_replication.inc

--let $rpl_connection_name= server5
--source include/rpl_connection.inc
--let $wait_condition= SELECT COUNT(*)=3 FROM performance_schema.replication_asynchronous_connection_failover
--source include/wait_condition.inc

--echo
--echo # 8. Remove secondary member from group A and validate is removed from
--echo #    list of senders
--echo
--let $rpl_connection_name= server2
--source include/rpl_connection.inc
--source include/stop_group_replication.inc

--let $rpl_connection_name= server5
--source include/rpl_connection.inc
--let $wait_condition= SELECT COUNT(*)=2 FROM performance_schema.replication_asynchronous_connection_failover
--source include/wait_condition.inc

--echo
--echo # 9. Clean up.
--echo
--let $rpl_connection_name= server5
--source include/rpl_connection.inc

SET SESSION sql_log_bin = 0;
call mtr.add_suppression("The source .* for channel 'ch1' has joined the group .*, and so added its entry into replication_asynchronous_connection_failover table.");
call mtr.add_suppression("The source .* for channel 'ch1' has left the group .*, and so removed its entry from replication_asynchronous_connection_failover table.");
call mtr.add_suppression("The group .* for the channel .* has been removed, and so removed its entry from replication_asynchronous_connection_failover_managed and all the group members from replication_asynchronous_connection_failover table.");
call mtr.add_suppression("The group .* for the channel .* has been added, and so added its entry in replication_asynchronous_connection_failover_managed and source to replication_asynchronous_connection_failover table.");
SET SESSION sql_log_bin = 1;

--let $rpl_channel_name='ch1'
--source include/stop_slave.inc

--let $rpl_reset_slave_all= 1
--let $rpl_multi_source= 1
--source include/rpl_reset_slave.inc
--let $rpl_channel_name=
--let $rpl_reset_slave_all=
--let $rpl_multi_source=

--replace_result $group_a GROUP_A
--eval SELECT asynchronous_connection_failover_delete_managed('ch1', '$group_a')

--replace_result $group_b GROUP_B
--eval SELECT asynchronous_connection_failover_delete_managed('ch1', '$group_b')

--let $rpl_connection_name= server3
--source include/rpl_connection.inc
--source include/stop_group_replication.inc

--let $rpl_connection_name= server1
--source include/rpl_connection.inc
--source include/stop_group_replication.inc

--source include/group_replication_end.inc
