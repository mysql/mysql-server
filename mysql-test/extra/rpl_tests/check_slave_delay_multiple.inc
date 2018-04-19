### Purpose ###
#
# This test file is invoked from rpl_sql_delay_multiple.
#
# After synchronizing both slaves with the master, we check that the slave is no longer
# waiting for the delay.
# Then, we verify that the slave's only starts applying the transaction after
# the delay is observed, through its immediate_commit_timestamp.
#
### Parameters ###
#
# $trx_num

--let $sync_slave_connection= server_2
--source include/sync_slave_sql_with_master.inc

--let $rpl_connection_name= server_1
--source include/rpl_connection.inc

--let $sync_slave_connection= server_3
--source include/sync_slave_sql_with_master.inc

--let $rpl_connection_name= server_1
--source include/rpl_connection.inc

--let $server_uuid= query_get_value(SELECT @@global.server_uuid, @@global.server_uuid, 1)
--let $gtid= $server_uuid:$trx_num
--source include/get_immediate_commit_timestamp.inc
--let $master_immediate_commit_timestamp= `SELECT $immediate_commit_timestamp`

--let $rpl_connection_name= server_2
--source include/rpl_connection.inc

--source include/get_immediate_commit_timestamp.inc
--let $slave1_immediate_commit_timestamp= `SELECT $immediate_commit_timestamp`
--let $sql_delay_1= query_get_value("SHOW SLAVE STATUS", SQL_Delay, 1)
#ceiling is also used when computing the delay in the code
--let $timestamp_1_diff= `SELECT CEILING(($slave1_immediate_commit_timestamp-$master_immediate_commit_timestamp)/ 1000000)`

--let $rpl_connection_name= server_3
--source include/rpl_connection.inc

--source include/get_immediate_commit_timestamp.inc
--let $slave2_immediate_commit_timestamp= `SELECT $immediate_commit_timestamp`
--let $sql_delay_2= query_get_value("SHOW SLAVE STATUS", SQL_Delay, 1)
#ceiling is also used when computing the delay in the code
--let $timestamp_2_diff= `SELECT CEILING(($slave2_immediate_commit_timestamp-$master_immediate_commit_timestamp)/ 1000000)`

--let $assert_text= The difference between the immediate_commit_timestamp should be at least the SQL_Delay
--let $assert_cond= $timestamp_1_diff >= $sql_delay_1
--source include/assert.inc

--let $assert_text= The difference between the immediate_commit_timestamp should be at least the SQL_Delay
--let $assert_cond= $timestamp_2_diff >= $sql_delay_2
--source include/assert.inc

--let $rpl_connection_name=server_1
--source include/rpl_connection.inc
