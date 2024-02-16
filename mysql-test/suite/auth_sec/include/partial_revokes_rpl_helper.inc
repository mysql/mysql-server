if($TEST_MODE == RPL)
{
--connection default
--source include/sync_slave_sql_with_master.inc
--connection slave
--echo [connection slave]
--let $user = bar
--let $reset_user = 1
--source include/show_grants.inc
--connection $conn_name
--echo [connection $conn_name]
}
