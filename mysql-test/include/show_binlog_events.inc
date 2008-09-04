# $binlog_start can be set by caller or take a default value

if (!$binlog_start)
{
  let $binlog_start=107;
}
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR $binlog_start <binlog_start>
--replace_column 2 # 4 # 5 #
--replace_regex /\/\* xid=.* \*\//\/* XID *\// /table_id: [0-9]+/table_id: #/ /file_id=[0-9]+/file_id=#/ /block_len=[0-9]+/block_len=#/
--eval show binlog events from $binlog_start
