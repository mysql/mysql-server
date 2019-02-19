--let $_check_log_bin= query_get_value(SHOW GLOBAL VARIABLES LIKE 'log_bin', Value, 1)
if ($_check_log_bin == ON)
{
  if (`SELECT @@binlog_transaction_compression = TRUE`)
  {
    --skip Doesn't support --binlog-transaction-compression = TRUE
  }
}
