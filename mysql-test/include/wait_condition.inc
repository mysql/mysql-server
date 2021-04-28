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
#    [let $explicit_default_wait_timeout= N] # to override the default reset
#    --source include/wait_condition.inc
#
#   OR
#
#    let $silent_failure=1 # Ignore the echo when the wait times out
#    let $wait_timeout= 60; # Override default 30 seconds with 60.
#    let $wait_condition=
#      SELECT c = 3 FROM t;
#    --source include/wait_condition.inc
#    --echo Executed the test condition $wait_condition_reps times
#
#
# EXAMPLE
#    events_bugs.test, events_time_zone.test
#

let $wait_counter= 300;
if ($wait_timeout)
{
  let $wait_counter= `SELECT $wait_timeout * 10`;
}
# Reset $wait_timeout so that its value won't be used on subsequent
# calls, and default will be used instead.
if ($explicit_default_wait_timeout)
{
  --let $wait_timeout= $explicit_default_wait_timeout
}
if (!$explicit_default_wait_timeout)
{
  --let $wait_timeout= 0
}

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
        sleep 0.1;
        dec $wait_counter;
    }
}
if (!$success)
{
  if (!$silent_failure)
  {
    echo Timeout in wait_condition.inc for $wait_condition;
  }
  if ($show_rpl_debug_info)
  {
    --source include/show_rpl_debug_info.inc
    --die Timeout in wait_condition.inc for $wait_condition
  }
}
