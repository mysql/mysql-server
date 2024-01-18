# ==== Purpose ====
#
# Run a query over and over until it succeeds ot timeout occurs
#
# ==== Usage ====
#
# --let $query= STATEMENT
# --source include/wait_for_query_to_succeed.inc
#
# Parameters:
#   $query
#     The statement to execute.


let $counter= 100;

disable_abort_on_error;
disable_query_log;
disable_result_log;
eval $query;
while ($mysql_errno)
{
  eval $query;
  sleep 0.1;
  dec $counter;

  if (!$counter)
  {
    --source include/rpl/debug/show_debug_info.inc
    --die "Waited too long for query to succeed";
  }
}
enable_abort_on_error;
enable_query_log;
enable_result_log;
