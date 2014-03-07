# Prerequisites:
# connections:
#   cq - connection where queries are run
#   ce - connection where explain is run
# variables:
#   query - query to explain
#   format - explain format: 'json' for json, or traditional otherwise
#   point  - syncing point

--disable_reconnect

connection cq;
--disable_result_log
--disable_query_log
let $QID= `SELECT CONNECTION_ID()`;
eval SET DEBUG_SYNC= '$point SIGNAL ready_for_explain WAIT_FOR explained';
send_eval $query;

connection ce;
--disable_result_log
--disable_query_log
if ($format == 'json') {
  let $fmt= FORMAT=JSON;
}
if ($format != 'json') {
  let $fmt= FORMAT=TRADITIONAL;
}
--enable_result_log
--echo EXPLAIN $fmt FOR QUERY '$query'
# Let the query being explained reach the sync point
SET DEBUG_SYNC= 'now WAIT_FOR ready_for_explain';
SET DEBUG_SYNC= 'after_explain_other SIGNAL explained';
--error $err
--eval EXPLAIN $fmt FOR CONNECTION $QID;

connection cq;
--disable_result_log
--disable_query_log
reap;
SET DEBUG_SYNC= 'RESET';
--enable_query_log
--enable_result_log

--enable_reconnect

