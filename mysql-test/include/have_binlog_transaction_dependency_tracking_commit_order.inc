--source include/have_log_bin.inc

--disable_warnings
let $have_binlog_transaction_dependency_tracking= `SELECT @@binlog_transaction_dependency_tracking LIKE 'COMMIT_ORDER'`;
--enable_warnings
if (!$have_binlog_transaction_dependency_tracking)
{
  --skip Test requires binlog_transaction_dependency_tracking = COMMIT_ORDER
}
