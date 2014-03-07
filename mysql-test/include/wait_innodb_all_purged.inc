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
--source include/have_innodb.inc
--source include/have_debug.inc

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
while ($wait_counter)
{
    let $trx_age = `SELECT VARIABLE_VALUE FROM INFORMATION_SCHEMA.GLOBAL_STATUS
WHERE VARIABLE_NAME = 'INNODB_PURGE_TRX_ID_AGE';`;

    if ($trx_age != $prev_trx_age)
    {
        let $wait_counter= $wait_counter_init;
        let $prev_trx_age= $trx_age;
    }

    let $success= `SELECT $trx_age < 1`;
    inc $wait_condition_reps;
    if ($success)
    {
        let $wait_counter= 0;
    }
    if (!$success)
    {
        set global innodb_purge_run_now=ON;
        real_sleep 0.1;
        dec $wait_counter;
    }
}
if (!$success)
{
  echo Timeout in wait_innodb_all_purged.inc for INNODB_PURGE_TRX_ID_AGE = $trx_age;
}

--enable_query_log
