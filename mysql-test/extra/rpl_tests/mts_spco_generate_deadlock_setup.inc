if ($mts_spco_gd_transaction_retries == '')
{
  --let $mts_spco_gd_transaction_retries = 0
}
if ($mts_spco_gd_innodb_wait_timeout == '')
{
  --let $mts_spco_gd_innodb_wait_timeout = 5
}

--source include/rpl_connection_slave.inc
--source include/stop_slave_sql.inc

set session sql_log_bin=0;
CALL mtr.add_suppression(".*Worker.*failed executing transaction.*at master log .*, end_log_pos.*Deadlock found when trying to get lock.*");
CALL mtr.add_suppression(".*Worker.*failed executing transaction.*at master log .*, end_log_pos.*Can not lock user management caches for processing.*");
CALL mtr.add_suppression(".*Worker.*failed executing transaction.*at master log .*, end_log_pos.*Replica worker has stopped after at least one previous worker.*");
CALL mtr.add_suppression(".*worker thread retried transaction.*time.*in vain, giving up.*");
CALL mtr.add_suppression(".*The slave coordinator and worker threads are stopped.*");
CALL mtr.add_suppression(".*Slave worker has stopped after at least one previous worker encountered an error when replica-preserve-commit-order was enabled.*");
set session sql_log_bin=1;

--let $saved_replica_parallel_type = `SELECT @@GLOBAL.replica_parallel_type`
--let $saved_replica_parallel_workers = `SELECT @@GLOBAL.replica_parallel_workers`
--let $saved_replica_preserve_commit_order = `SELECT @@GLOBAL.replica_preserve_commit_order`
--let $saved_replica_transaction_retries = `SELECT @@GLOBAL.replica_transaction_retries`
--let $saved_innodb_lock_wait_timeout = `SELECT @@GLOBAL.innodb_lock_wait_timeout`
--let $saved_read_only = `SELECT @@GLOBAL.read_only`
SET GLOBAL replica_parallel_type = LOGICAL_CLOCK;
SET GLOBAL replica_parallel_workers = 3;
SET GLOBAL replica_preserve_commit_order = ON;
--replace_result $mts_spco_gd_transaction_retries REPLICA_TRANSACTION_RETRIES
--eval SET GLOBAL replica_transaction_retries = $mts_spco_gd_transaction_retries
--replace_result $mts_spco_gd_innodb_wait_timeout INNODB_LOCK_WAIT_TIMEOUT
--eval SET GLOBAL innodb_lock_wait_timeout = $mts_spco_gd_innodb_wait_timeout

--let $rpl_connection_name = rpl_slave_connection_2
--let $rpl_server_number = 2
--source include/rpl_connect.inc
--let $rpl_connection_name = rpl_slave_connection_3
--let $rpl_server_number = 2
--source include/rpl_connect.inc
