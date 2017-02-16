--source include/have_log_bin.inc

if (`SELECT @@binlog_format = 'STATEMENT'`)
{
  --skip Neither MIXED nor ROW binlog format
}
