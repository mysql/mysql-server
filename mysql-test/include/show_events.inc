##############################################################################
# It's an auxiliary file used to show binary log events or relay log events.
# It is only called by show_binlog_events.inc and show_relaylog_events.inc.
##############################################################################

if (!$binlog_start)
{
  # If $binlog_start is not set, we will set it as the second event's
  # position.  The first event(Description Event) is always ignored. For
  # description event's length might be changed because of adding new events,
  # 'SHOW BINLOG EVENTS LIMIT 1' is used to get the right value.
  --let $binlog_start= query_get_value(SHOW BINLOG EVENTS LIMIT 1, End_log_pos, 1)
}

--let $_statement=show binlog events
if ($is_relay_log)
{
  --let $_statement=show relaylog events
}

if ($binlog_file)
{
  --let $_statement= $_statement in '$binlog_file'
}

--let $_statement= $_statement from $binlog_start

# Cannot use if($binlog_limit) since the variable may begin with a 0

if (`SELECT '$binlog_limit' <> ''`)
{
  --let $_statement= $_statement limit $binlog_limit
}

--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR $binlog_start <binlog_start>
--replace_column 2 # 4 # 5 #
--replace_regex /\/\* xid=.* \*\//\/* XID *\// /table_id: [0-9]+/table_id: #/ /file_id=[0-9]+/file_id=#/ /block_len=[0-9]+/block_len=#/ /Server ver:.*$/SERVER_VERSION, BINLOG_VERSION/
--eval $_statement
