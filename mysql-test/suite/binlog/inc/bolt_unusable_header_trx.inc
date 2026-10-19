# Commits 3000 rows of 4000 bytes in one transaction, one small row event per
# row, and finds the transaction's Large_transaction_header event.
#
# Parameters:
#   $bolt_id_base  the rows get the ids $bolt_id_base + 1 to $bolt_id_base + 3000
#
# Sets:
#   $bolt_file     the promoted binlog file
#   $bolt_lth_pos  start offset of the Large_transaction_header event
#   $bolt_lth_end  offset of the event after it
#
if (!$bolt_id_base)
{
  --let $bolt_id_base = 0
}
--disable_query_log
SET SESSION cte_max_recursion_depth = 10000;
BEGIN;
--eval INSERT INTO t1 WITH RECURSIVE s(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM s WHERE n < 3000) SELECT n + $bolt_id_base, REPEAT('a', 4000) FROM s
COMMIT;
--enable_query_log
--let $bolt_file = query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $bolt_lth_pos =
--let $bolt_event_index = 1
while (!$bolt_lth_pos)
{
  --let $bolt_event_type = query_get_value(SHOW BINLOG EVENTS IN '$bolt_file', Event_type, $bolt_event_index)
  if ($bolt_event_type == Large_transaction_header)
  {
    --let $bolt_lth_pos = query_get_value(SHOW BINLOG EVENTS IN '$bolt_file', Pos, $bolt_event_index)
    --let $bolt_lth_end = query_get_value(SHOW BINLOG EVENTS IN '$bolt_file', End_log_pos, $bolt_event_index)
  }
  if ($bolt_event_index == 10)
  {
    --die The promoted binlog file has no Large_transaction_header event.
  }
  --inc $bolt_event_index
}
