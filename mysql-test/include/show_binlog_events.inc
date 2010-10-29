##############################################################################
# Show binary log events 
#
# Useage: 
# let $binlog_file= master-bin.000002; 
# let $binlog_start= 106; 
# let $binlog_limit= 1, 3; 
# source include/show_binlog_events.inc;
#
# It shows the first binary log file if $binlog_file is not given. 
#
# It shows events from the end position of the description event if
# $binlog_start is not given.
#
# It shows all of the events if $binlog_limit is not given. 
# $binlog_format has the same semantic with 'LIMIT' option.
#
##############################################################################

if (!$binlog_start)
{
  # If $binlog_start is not set, we will set it as the second event's position.
  # The first event(Description Event) is always ignored. For description
  # event's length might be changed because of adding new events, 'SHOW BINLOG
  # EVENTS LIMIT 1' is used to get the right value.
  --let $binlog_start= query_get_value(SHOW BINLOG EVENTS LIMIT 1, End_log_pos, 1)
}

--let $_statement=show binlog events
if ($binlog_file)
{
  --let $_statement= $_statement in '$binlog_file'
}

--let $_statement= $_statement from $binlog_start

if ($binlog_limit)
{
  --let $_statement= $_statement limit $binlog_limit
}

--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR $binlog_start <binlog_start>
--replace_column 2 # 4 # 5 #
--replace_regex /\/\* xid=.* \*\//\/* XID *\// /table_id: [0-9]+/table_id: #/ /file_id=[0-9]+/file_id=#/ /block_len=[0-9]+/block_len=#/
--eval $_statement
