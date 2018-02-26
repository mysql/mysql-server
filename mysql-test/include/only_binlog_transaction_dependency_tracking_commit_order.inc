if (`SELECT @@GLOBAL.binlog_transaction_dependency_tracking != 'COMMIT_ORDER'`)
{
  --skip Test requires binlog_transaction_dependency_tracking= COMMIT_ORDER
}
