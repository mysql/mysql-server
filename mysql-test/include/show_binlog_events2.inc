if (!$binlog_start)
{
  let $binlog_checksum= `select variable_value='ON' from information_schema.GLOBAL_VARIABLES where variable_name='binlog_checksum';`;

  if($binlog_checksum)
  {
    let $binlog_start=112;
  }

  if(!$binlog_checksum)
  {
    let $binlog_start=107;
  }
}

--replace_result $binlog_start <binlog_start>
--replace_column 2 # 5 #
--replace_regex /\/\* xid=.* \*\//\/* XID *\// /table_id: [0-9]+/table_id: #/
--eval show binlog events from $binlog_start
