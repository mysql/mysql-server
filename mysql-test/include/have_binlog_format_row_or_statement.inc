source include/have_log_bin.inc;


if (`SELECT @@binlog_format = 'MIXED'`)
{
  --skip Neither ROW nor STATEMENT binlog format
}

