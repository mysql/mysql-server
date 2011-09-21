#--let $binlog_start=107
--let $binlog_on= query_get_value("SELECT Variable_Value='ON' as binlog from INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE Variable_name like 'log_bin'", binlog, 1)
--let $binlog_start= query_get_value(SHOW BINLOG EVENTS LIMIT 1, End_log_pos, 1)
if (!$binlog_on)
{
  # Where there's no Binlog, we can use any old value
  --let $binlog_start= 0
}
--replace_result $binlog_start <binlog_start>
--replace_column 2 # 5 #
--replace_regex /\/\* xid=.* \*\//\/* XID *\// /table_id: [0-9]+/table_id: #/
--eval show binlog events from $binlog_start
