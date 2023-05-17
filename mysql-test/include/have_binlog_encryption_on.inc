--source include/have_log_bin.inc

if (`SELECT @@GLOBAL.binlog_encryption = FALSE`)
{
  --skip Test requires --binlog-encryption = TRUE
}
