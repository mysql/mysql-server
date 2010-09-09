#
# SUMMARY
#   Check if statement affecting or reading table '$table' doesn't
#   take any kind of locks on its rows.
#
# PARAMETERS 
#   $table      Table for which presence of row locks should be checked.
#   $con_aux    Name of auxiliary connection to be used by this script.
#   $statement  Statement to be checked.
#
# EXAMPLE
#    innodb_mysql_lock2.test
#
--disable_result_log
--disable_query_log

connection default;
begin;
--eval select * from $table for update;

connection $con_aux;
begin;
--send_eval $statement;

--enable_result_log
--enable_query_log

connection default;
# Wait until statement is successfully executed while
# all rows in table are X-locked. This means that it
# does not acquire any row locks.
# We use wait_condition.inc instead of simply reaping 
# statement here in order to avoid deadlocks if test
# fails and to time out gracefully instead.
let $wait_condition=
  select count(*) = 0 from information_schema.processlist
  where info = "$statement";
--source include/wait_condition.inc

--disable_result_log
--disable_query_log

if ($success)
{
# Apparently statement was successfully executed and thus it
# has not required any row locks.
# To be safe against wait_condition.inc succeeding due to
# races let us first reap the statement being checked to
# ensure that it has been successfully executed.
connection $con_aux;
--reap
rollback;
connection default;
rollback;
--echo Success: '$statement' doesn't take row locks on '$table'.
}
if (!$success)
{
# Waiting has timed out. Apparently statement was blocked on
# some row lock. So to be able to continue we need to unlock
# rows first.
rollback;
connection $con_aux;
--reap
rollback;
connection default;
--echo Error: '$statement' takes some row locks on '$table'!
}

--enable_result_log
--enable_query_log
