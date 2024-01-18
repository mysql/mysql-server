### Purpose ###
#
# This test file is invoked from rpl_sql_delay_old.
#
# After sleeping for $slave_sleep seconds (less that $delay), we check that the
# slave is still waiting for SOURCE_DELAY.
# After synchronizing with the master, we check that the slave is no longer
# waiting for the delay.
# Then, we check that the delay was applied.
#
#
### Parameters ###
#
# $slave_sleep  number of seconds the slave sleeps before checking if it is
#               still waiting for the delay
# $delay        number of seconds configured for slave delay


--source include/rpl/connection_replica.inc

--sleep $slave_sleep

--let $remaining_delay=query_get_value(Select *from performance_schema.replication_applier_status,REMAINING_DELAY,1)
--let $sql_remaining_delay=query_get_value(SHOW REPLICA STATUS,SQL_Remaining_Delay,1)

# Assert that the remaining delay value shown by both show replica status
# and replication_applier_status table of performance schema is same.
# using '>=' since $remaining_delay is obtained before $sql_remaining_delay.

--let $assert_text= Assert that the REMAINING_DELAY from performance_schema is same as SQL_Remaining_Delay in the output of show replica status.
--let $assert_cond= "$remaining_delay" >= "$sql_remaining_delay"
--source include/assert.inc

--source include/rpl/connection_source.inc
--source include/rpl/sync_to_replica.inc

--source include/rpl/connection_source.inc
--let $time_2= `SELECT SYSDATE(6)`

--source include/rpl/connection_replica.inc
--let $ms_diff= `SELECT TIMESTAMPDIFF(MICROSECOND,'$time_1','$time_2')`
# Ceiling is also used when computing the delay in the code
--let $time_diff= `SELECT CEILING($ms_diff / 1000000)`
--let $assert_text= In the old infrastructure, the execution time of the transaction in the slave must be at least the delay
--let $assert_cond= $time_diff >= $delay
--source include/assert.inc

--let $assert_text= Status should not be 'Waiting until SOURCE_DELAY seconds after source executed event'
--let $assert_cond= NOT("[SHOW REPLICA STATUS, Replica_SQL_Running_State, 1]" LIKE "Waiting until % seconds after source executed event")
--source include/assert.inc

--source include/rpl/connection_source.inc
# $time_1 is collected in the same server as $time_2
--let $time_1= `SELECT SYSDATE(6)`
