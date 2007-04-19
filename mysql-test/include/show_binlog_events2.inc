--replace_result $start_pos <start_pos> $end_pos <end_pos>
--replace_column 2 #
--replace_regex /\/\* xid=.* \*\//\/* XID *\// /table_id: [0-9]+/table_id: #/
--eval show binlog events in '$log_name'  from $start_pos limit 1,1

