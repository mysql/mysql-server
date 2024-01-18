#
# Run a query over and over until it fails or timeout occurs
#


let $counter= 100;

disable_abort_on_error;
disable_query_log;
disable_result_log;
eval $query;
while (!$mysql_errno)
{
  eval $query;
  sleep 0.1;
  dec $counter;

  if (!$counter)
  {
    if ($show_rpl_debug_info)
    {
      --source include/rpl/debug/show_debug_info.inc
    }
    --die "Waited too long for query to fail";
  }
}
enable_abort_on_error;
enable_query_log;
enable_result_log;
