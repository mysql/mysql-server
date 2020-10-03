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

CALL mtr.add_suppression(".*Worker.*failed executing transaction.*at master log .*, end_log_pos.*Deadlock found when trying to get lock.*");
CALL mtr.add_suppression(".*Worker.*failed executing transaction.*at master log .*, end_log_pos.*Can not lock user management caches for processing.*");
CALL mtr.add_suppression(".*Worker.*failed executing transaction.*at master log .*, end_log_pos.*Replica worker has stopped after at least one previous worker.*");
CALL mtr.add_suppression(".*worker thread retried transaction.*time.*in vain, giving up.*");
CALL mtr.add_suppression(".*The slave coordinator and worker threads are stopped.*");
CALL mtr.add_suppression(".*Slave worker has stopped after at least one previous worker encountered an error when slave-preserve-commit-order was enabled.*");

--let $saved_replica_parallel_type = `SELECT @@GLOBAL.slave_parallel_type`
--let $saved_replica_parallel_workers = `SELECT @@GLOBAL.slave_parallel_workers`
--let $saved_replica_preserve_commit_order = `SELECT @@GLOBAL.slave_preserve_commit_order`
--let $saved_replica_transaction_retries = `SELECT @@GLOBAL.slave_transaction_retries`
--let $saved_innodb_lock_wait_timeout = `SELECT @@GLOBAL.innodb_lock_wait_timeout`
--let $saved_read_only = `SELECT @@GLOBAL.read_only`
SET GLOBAL slave_parallel_type = LOGICAL_CLOCK;
SET GLOBAL slave_parallel_workers = 3;
SET GLOBAL slave_preserve_commit_order = ON;
--replace_result $mts_spco_gd_transaction_retries REPLICA_TRANSACTION_RETRIES
--eval SET GLOBAL slave_transaction_retries = $mts_spco_gd_transaction_retries
--replace_result $mts_spco_gd_innodb_wait_timeout INNODB_LOCK_WAIT_TIMEOUT
--eval SET GLOBAL innodb_lock_wait_timeout = $mts_spco_gd_innodb_wait_timeout
