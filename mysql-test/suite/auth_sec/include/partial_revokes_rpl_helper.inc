if($TEST_MODE == RPL)
{
--connection default
--source include/sync_slave_sql_with_master.inc
--connection slave
--echo [connection slave]
SHOW GRANTS FOR bar;
--connection $conn_name
--echo [connection $conn_name]
}
