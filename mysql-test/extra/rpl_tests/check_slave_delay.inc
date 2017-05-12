### Purpose ###
#
# This test file is invoked from rpl_sql_delay.
#
# After sleeping for $slave_sleep seconds (less that $delay), we check that the
# slave is still waiting for MASTER_DELAY.
# After synchronizing with the master, we check that the slave is no longer
# waiting for the delay.
# Then, we verify that the slave's only starts applying the transaction after
# the delay is observed, through its immediate_commit_timestamp.
#
#
### Parameters ###
#
# $slave_sleep  number of seconds the slave sleeps before checking if it is
#               still waiting for the delay
# $delay        number of seconds configured for slave delay
# $trx_num      the gno of the transaction's gtid


--source include/rpl_connection_slave.inc

--sleep $slave_sleep

--let $remaining_delay= query_get_value(SELECT REMAINING_DELAY FROM performance_schema.replication_applier_status, REMAINING_DELAY, 1)
--let $sql_remaining_delay= query_get_value(SHOW SLAVE STATUS, SQL_Remaining_Delay, 1)

# Assert that the remaining delay value shown by both show slave status
# and replication_applier_status table of performance schema is same.
# using '>=' since $remaining_delay is obtained before $sql_remaining_delay.

--let $assert_text= Assert that the REMAINING_DELAY from performance_schema is same as SQL_Remaining_Delay in the output of show slave status.
--let $assert_cond= "$remaining_delay" >= "$sql_remaining_delay"
--source include/assert.inc

--source include/rpl_connection_master.inc
--source include/sync_slave_sql_with_master.inc

--let $assert_text= Status should not be 'Waiting until MASTER_DELAY seconds after master executed event'
--let $assert_cond= NOT("[SHOW SLAVE STATUS, Slave_SQL_Running_State, 1]" LIKE "Waiting until % seconds after master executed event")
--source include/assert.inc

--source include/rpl_connection_master.inc
--let $server_uuid= query_get_value(SELECT @@global.server_uuid, @@global.server_uuid, 1)
--let $gtid= $server_uuid:$trx_num
--source include/get_immediate_commit_timestamp.inc
--let $master_immediate_commit_timestamp= $immediate_commit_timestamp

--source include/rpl_connection_slave.inc
--let $gtid= $server_uuid:$trx_num
--source include/get_immediate_commit_timestamp.inc
--let $slave_immediate_commit_timestamp= $immediate_commit_timestamp
--let $sql_delay= query_get_value("SHOW SLAVE STATUS", SQL_Delay, 1)
# Ceiling is also used when computing the delay in the code
--let $timestamp_diff_ms= `SELECT $slave_immediate_commit_timestamp-$master_immediate_commit_timestamp`
--let $timestamp_diff_sec= `SELECT CEILING($timestamp_diff_ms / 1000000)`

--let $assert_text= The difference between the immediate_commit_timestamp should be at least the SQL_Delay
--let $assert_cond= $timestamp_diff_sec >= $sql_delay
--source include/assert.inc

--source include/rpl_connection_master.inc
