# include/wait_innodb_all_purged.inc
#
# SUMMARY
#
#    Waits until purged all undo records of innodb, or operation times out.
#
# USAGE
#
#    --source include/wait_innodb_all_purged.inc
#

--let $is_debug= `select (version() like '%debug%') as debug`

if ($is_debug)
{
--disable_query_log

let $wait_counter_init= 300;
if ($wait_timeout)
{
  let $wait_counter_init= `SELECT $wait_timeout * 10`;
}
# Reset $wait_timeout so that its value won't be used on subsequent
# calls, and default will be used instead.
let $wait_timeout= 0;

let $wait_counter= $wait_counter_init;

# Keep track of how many times the wait condition is tested
let $wait_condition_reps= 0;
let $prev_trx_age= 0;
let $age_log=0;
while ($wait_counter)
{
--disable_warnings
# Don't wait for purge blocked by active ReadView(s)
    let $trx_view_age = `SELECT VARIABLE_VALUE FROM performance_schema.global_status
WHERE VARIABLE_NAME = 'INNODB_PURGE_VIEW_TRX_ID_AGE';`;

    let $trx_age = `SELECT VARIABLE_VALUE FROM performance_schema.global_status
WHERE VARIABLE_NAME = 'INNODB_PURGE_TRX_ID_AGE';`;
--enable_warnings

    if ($trx_age != $prev_trx_age)
    {
        let $age_log=$age_log:$wait_condition_reps $trx_age;
        let $wait_counter= $wait_counter_init;
        let $prev_trx_age= $trx_age;
    }

    let $success= `SELECT $trx_age <= $trx_view_age`;
    inc $wait_condition_reps;
    if ($success)
    {
        let $wait_counter= 0;
    }
    if (!$success)
    {
        set global innodb_purge_run_now=ON;
        sleep 0.1;
        dec $wait_counter;
    }
}
if (!$success)
{
  let $age_log=$age_log:$wait_condition_reps;
  echo INNODB_PURGE_TRX_ID_AGE = $trx_age (previous id:count $age_log);
  die Timeout in wait_innodb_all_purged.inc;
}

--enable_query_log
}
