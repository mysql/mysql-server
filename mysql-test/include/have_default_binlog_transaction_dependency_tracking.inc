if (`SELECT @@global.binlog_transaction_dependency_tracking <> 'COMMIT_ORDER'`)
{
  --skip Test runs only with default value for binlog_transaction_dependency_tracking
}
