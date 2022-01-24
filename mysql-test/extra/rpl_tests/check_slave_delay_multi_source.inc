### Purpose ###
#
# This test file is invoked from rpl_sql_delay_multi_source.
#
# After synchronizing with the master, we check that the slave is no longer
# waiting for the delay.
# Then, we verify that the slave's only starts applying the transaction after
# the delay is observed, through its immediate_commit_timestamp.
#
#
### Parameters ###
#
# $trx_num      the gno of the transaction's gtid
# $slave        slaves connection
# $master       masters connection

--let $sync_slave_connection= $slave
--source include/sync_slave_sql_with_master.inc

--let $assert_text= Status should not be 'Waiting until SOURCE_DELAY seconds after source executed event'
--let $assert_cond= NOT("[SHOW SLAVE STATUS, Slave_SQL_Running_State, 1]" LIKE "Waiting until % seconds after source executed event")
--source include/assert.inc

--let $rpl_connection_name= $master
--source include/rpl_connection.inc

--let $server_uuid= query_get_value(SELECT @@global.server_uuid, @@global.server_uuid, 1)
--let $gtid= $server_uuid:$trx_num
--source include/get_immediate_commit_timestamp.inc
--let $master_immediate_commit_timestamp= `SELECT $immediate_commit_timestamp`

--let $rpl_connection_name= $slave
--source include/rpl_connection.inc

--let $gtid= $server_uuid:$trx_num
--source include/get_immediate_commit_timestamp.inc
--let $slave_immediate_commit_timestamp= `SELECT $immediate_commit_timestamp`
--let $sql_delay= query_get_value("SHOW SLAVE STATUS", SQL_Delay, 1)
#ceiling is also used when computing the delay in the code
--let $timestamp_diff= `SELECT CEILING(($slave_immediate_commit_timestamp-$master_immediate_commit_timestamp)/1000000)`

--let $assert_text= The difference between the immediate_commit_timestamp should be at least the SQL_Delay
--let $assert_cond= $timestamp_diff >= $sql_delay
--source include/assert.inc

--let $rpl_connection_name= $master
--source include/rpl_connection.inc
