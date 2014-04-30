if (!$binlog_start)
{
  # If $binlog_start is not set, we will set it as the second event's
  # position.  The first event(Description Event) is always ignored. For
  # description event's length might be changed because of adding new events,
  # 'SHOW BINLOG EVENTS LIMIT 1' is used to get the right value.
  --let $binlog_start= query_get_value(SHOW BINLOG EVENTS LIMIT 1, End_log_pos, 1)
}
--replace_result $binlog_start <binlog_start>
--replace_column 2 # 5 #
--replace_regex /\/\* xid=.* \*\//\/* XID *\// /table_id: [0-9]+/table_id: #/ /SET @@SESSION.GTID_NEXT=.*$/SET @@SESSION.GTID_NEXT= 'GTID';/ /GROUPS: .*:(.*,.*)/GROUPS: GTID:(X,X)/ / \/\* XID \*\/// /Xid/Query/
--eval show binlog events from $binlog_start
