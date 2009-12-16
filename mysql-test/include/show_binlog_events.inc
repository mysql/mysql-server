# $binlog_start can be set by caller or take a default value
# $binary_log_file the name of the log file show
# $binary_log_limit_row - sets the number of binlog rows to be returned
# $binary_log_limit_offset - sets the offset where to start returning events

let $show_binlog_events= show binlog events;

if (!$binlog_start)
{
  # defaults to chop the first event in the binary log
  let $binlog_start=107;
}

if (!`SELECT '$binary_log_file' = ''`)
{
    let $show_binlog_events= $show_binlog_events in '$binary_log_file';
}
let $show_binlog_events= $show_binlog_events from $binlog_start;

if ($binary_log_limit_row)
{
  let $limit= limit;
  if ($binary_log_limit_offset)
  {
    let $limit= $limit $binary_log_limit_offset, ;
  }

  let $limit= $limit $binary_log_limit_row;
  let $show_binlog_events= $show_binlog_events $limit;
}

--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR $binlog_start <binlog_start>
--replace_column 2 # 4 # 5 #
--replace_regex /\/\* xid=.* \*\//\/* XID *\// /table_id: [0-9]+/table_id: #/ /file_id=[0-9]+/file_id=#/ /block_len=[0-9]+/block_len=#/
--eval $show_binlog_events
