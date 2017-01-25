--source include/have_log_bin.inc

let $have_binlog_format_row= `SELECT @@binlog_format LIKE 'ROW'`;
if (!$have_binlog_format_row)
{
  --skip Test requires binlog_format = ROW
}
