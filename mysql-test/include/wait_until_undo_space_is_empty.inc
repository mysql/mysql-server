#
# Wait for the undo tablespace name $inactive_undo_space to become empty
#
--disable_query_log
--disable_result_log

# Each inner loop is about 1 second.  Set the number of seconds to wait.
let $wait_seconds = 30;
if ($wait_timeout)
{
  let $wait_seconds = $wait_timeout;
}

let $first_loop = 1;
let $wait_timeout = 30;
let $set_inactive = ALTER UNDO TABLESPACE $inactive_undo_space SET INACTIVE;
let $wait_condition = SELECT count(*) = 1 FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
                      WHERE NAME = '$inactive_undo_space' AND STATE = 'empty';

# Keep track of how many times the wait condition is tested
let $repetitions = 0;
while ($wait_seconds)
{
  let $repeat = 10;

  if (!$first_loop)
  {
    eval $set_inactive;
  }
  let $first_loop = 0;

  while ($repeat)
  {
    let $success_wait = `$wait_condition`;
    inc $repetitions;
    if ($success_wait)
    {
      let $wait_seconds = 1;
      let $repeat = 1;
    }
    if (!$success_wait)
    {
        real_sleep 0.1;
    }
    dec $repeat;
  }
  dec $wait_seconds;
}

if (!$success_wait)
{
  --echo # Timeout waiting for $inactive_undo_space to become empty after $repetitions tries
}

--enable_result_log
--enable_query_log
