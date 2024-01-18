### Purpose ###
#
# This test file is invoked from rpl_sql_delay_line_topology.
#
# After synchronizing the servers' sql threads, we check that the delay was
# observed between server_1 and server_2 and then between server_2 and
# server_3 by comparing their immediate_commit_timestamps
#
#
### Parameters ###
#
# $slave_sleep  number of seconds the slave sleeps before checking if it is
#               still waiting for the delay
# $delay        number of seconds configured for slave delay
# $trx_num      the gno of the transaction's gtid



--let $rpl_connection_name= server_1
--source include/connection.inc
--source include/rpl/sync.inc

--let $rpl_connection_name= server_2
--source include/connection.inc
--source include/rpl/sync.inc

--let $rpl_connection_name= server_1
--source include/connection.inc
--let $server_uuid= query_get_value(select @@global.server_uuid, @@global.server_uuid, 1)
--let $gtid= $server_uuid:$trx_num
--source include/rpl/get_immediate_commit_timestamp.inc
--let $server_1_ICT= `SELECT $immediate_commit_timestamp`

--let $rpl_connection_name= server_2
--source include/connection.inc
--source include/rpl/get_immediate_commit_timestamp.inc
--let $server_2_ICT= `SELECT $immediate_commit_timestamp`

--let $rpl_connection_name= server_3
--source include/connection.inc
--source include/rpl/get_immediate_commit_timestamp.inc
--let $server_3_ICT= `SELECT $immediate_commit_timestamp`

#ceiling is also used when computing the delay in the code
--let $ts_diff_server_2_1= `SELECT CEILING(($server_2_ICT-$server_1_ICT)/1000000)`
--let $ts_diff_server_3_2= `SELECT CEILING(($server_3_ICT-$server_2_ICT)/1000000)`

--let $assert_text= The difference between the immediate_commit_timestamp of servers 1 and 2 should be at least the SQL_Delay
--let $assert_cond= $ts_diff_server_2_1 >= $delay
--source include/assert.inc

--let $assert_text= The difference between the immediate_commit_timestamp of servers 2 and 3 should be at least the SQL_Delay
--let $assert_cond= $ts_diff_server_3_2 >= $delay
--source include/assert.inc

--let $rpl_connection_name= server_1
--source include/connection.inc
