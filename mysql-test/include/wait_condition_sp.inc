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
#   OR
#
#    let $wait_timeout= 60; # Override default 30 seconds with 60.
#    let $wait_condition=
#      SELECT c = 3 FROM t;
#    --source include/wait_condition.inc
#    --echo Executed the test condition $wait_condition_reps times
#
# EXAMPLE
#    events_bugs.test, events_time_zone.test
#

--disable_query_log

let $wait_counter= 300;
if ($wait_timeout)
{
  let $wait_counter= `SELECT $wait_timeout * 10`;
}
# Reset $wait_timeout so that its value won't be used on subsequent
# calls, and default will be used instead.
let $wait_timeout= 0;

# Keep track of how many times the wait condition is tested
# This is used by some tests (e.g., main.status)
let $wait_condition_reps= 0;
while ($wait_counter)
{
    let $success= `$wait_condition`;
    inc $wait_condition_reps;
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
  show master status;
  show slave status;
}

--enable_query_log

