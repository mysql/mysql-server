# include/wait_condition.inc
#
# SUMMARY
#
#    Waits until the passed statement returns true, or the operation
#    times out.
#
# USAGE
#
#    let $wait_condition=
#      SELECT c = 3 FROM t;
#    --source include/wait_condition.inc
#
# EXAMPLE
#    events_bugs.test
#

--disable_query_log

let $wait_counter= 300;
while ($wait_counter)
{
    let $success= `$wait_condition`;
    if ($success)
    {
        let $wait_counter= 0;
    }
    if (!$success)
    {
        real_sleep 0.1;
        dec $wait_counter;
    }
}
if (!$success)
{
  echo Timeout in wait_condition.inc for $wait_condition;
}

--enable_query_log
