########################################################
# Author: Jeb
# Date:   2007/04
# Purpose: To select out 1 row from offset 1
#          from the start position in the binlog whose
#          name is = log_name
######################################################## 

if (!$start_pos)
{
  # If $start_pos is not set, we will set it as the second event's
  # position.  The first event(Description Event) is always ignored. For
  # description event's length might be changed because of adding new events,
  # 'SHOW BINLOG EVENTS LIMIT 1' is used to get the right value.
  --let $start_pos= query_get_value(SHOW BINLOG EVENTS LIMIT 1, End_log_pos, 1)
}
if (!$off_set)
{
  # If $off_set not in use, set it to 0
  --let $off_set= 0
}
--replace_result $start_pos <start_pos>
--replace_column 2 # 5 <end_pos>
--replace_regex /\/\* xid=.* \*\//\/* XID *\// /table_id: [0-9]+/table_id: #/
--eval show binlog events in '$log_name'  from $start_pos limit $off_set,1

