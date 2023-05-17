--source include/have_log_bin.inc

if (`SELECT @@binlog_transaction_compression = FALSE`)
{
  --skip Test requires --binlog-transaction-compression = TRUE
}
