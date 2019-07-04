# include/wait_condition.inc
#
# SUMMARY
#
#    Waits until the passed statement returns true until the timeout.
#    Retry the sql_command once every second.
#    If the operation times out, report and return.
#
# USAGE
#
#    let $sql_command= ALTER UNDO TABLESPACE u3 SET INACTIVE;
#    let $wait_condition= SELECT c = 3 FROM t;
#    [let $explicit_default_wait_timeout= N] # to override the default reset
#    --source include/wait_condition.inc
#
#   OR
#
#    let $wait_timeout= 60; # Override default 30 seconds with 60.
#    let $sql_command= ALTER UNDO TABLESPACE u3 SET INACTIVE;
#    let $wait_condition= SELECT c = 3 FROM t;
#    --source include/wait_condition.inc
#    --echo Executed the test condition $wait_condition_reps times
#
#
# EXAMPLE
#    innodb_undo.undo_tablespace_debug
#

let $wait_counter= 30;
if ($wait_timeout)
{
  let $wait_counter= `SELECT $wait_timeout`;
}

# Keep track of how many times the wait condition is tested
let $wait_condition_reps= 0;
let $success= 0;
--disable_query_log
while ($wait_counter)
{
  eval $sql_command;
  let $retry_counter= 10;
  while ($retry_counter)
  {
    let $success= `$wait_condition`;
    inc $wait_condition_reps;
    if ($success)
    {
        let $retry_counter= 0;
        let $wait_counter= 0;
    }
    if (!$success)
    {
        real_sleep 0.1;
        dec $retry_counter;
    }
  }
  if (!$success)
  {
    dec $wait_counter;
  }
}
--enable_query_log
if (!$success)
{
  echo Timeout in wait_condition.inc for $wait_condition;
}
