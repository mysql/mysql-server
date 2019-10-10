--source include/have_log_bin.inc

let $have_binlog_transaction_dependency_tracking= `SELECT @@binlog_transaction_dependency_tracking LIKE 'COMMIT_ORDER'`;
if (!$have_binlog_transaction_dependency_tracking)
{
  --skip Test requires binlog_transaction_dependency_tracking = COMMIT_ORDER
}
