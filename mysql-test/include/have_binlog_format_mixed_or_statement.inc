source include/have_log_bin.inc;


if (`SELECT @@binlog_format = 'ROW'`)
{
  --skip Neither MIXED nor STATEMENT binlog format
}
