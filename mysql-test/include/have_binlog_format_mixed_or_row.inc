--source include/have_log_bin.inc

let $have_binlog_format_mixed_or_row= `SELECT @@binlog_format LIKE 'MIXED'
                                       OR @@binlog_format LIKE 'ROW'`;
if (!$have_binlog_format_mixed_or_row)
{
  --skip Test requires binlog_format = MIXED or ROW
}
