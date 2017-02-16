--source include/have_log_bin.inc

if (`SELECT @@binlog_format != 'STATEMENT'`)
{
  --skip Not STATEMENT format
}
