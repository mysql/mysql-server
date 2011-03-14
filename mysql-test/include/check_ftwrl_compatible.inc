#
# SUMMARY
#   Check that a statement is compatible with FLUSH TABLES WITH READ LOCK.
#
# PARAMETERS 
#   $con_aux1     Name of the 1st aux connection to be used by this script.
#   $con_aux2     Name of the 2nd aux connection to be used by this script.
#   $statement    The statement to be checked.
#   $cleanup_stmt The statement to be run in order to revert effects of
#                 the statement to be checked.
#   $skip_3rd_chk Skip the 3rd stage of checking. The purpose of the third
#                 stage is to check that metadata locks taken by this
#                 statement are compatible with metadata locks taken
#                 by FTWRL.
#
# EXAMPLE
#   flush_read_lock.test
#
--disable_result_log
--disable_query_log

# Reset DEBUG_SYNC facility for safety.
set debug_sync= "RESET";

#
# First, check that the statement can be run under FTWRL.
#
flush tables with read lock;
--disable_abort_on_error
--eval $statement
--enable_abort_on_error
let $err= $mysql_errno;
if (!$err)
{
--echo Success: Was able to run '$statement' under FTWRL.
unlock tables;
if ($cleanup_stmt)
{
--eval $cleanup_stmt;
}
}
if ($err)
{
--echo Error: Wasn't able to run '$statement' under FTWRL!
unlock tables;
}

#
# Then check that this statement won't be blocked by FTWRL
# that is active in another connection.
#
connection $con_aux1;
flush tables with read lock;

connection default;
--send_eval $statement;

connection $con_aux1;

--enable_result_log
--enable_query_log
let $wait_condition=
  select count(*) = 0 from information_schema.processlist
  where info = "$statement";
--source include/wait_condition.inc
--disable_result_log
--disable_query_log

if ($success)
{
--echo Success: Was able to run '$statement' with FTWRL active in another connection.

connection default;
# Apparently statement was successfully executed and so
# was not blocked by FTWRL.
# To be safe against wait_condition.inc succeeding due to
# races let us first reap the statement being checked to
# ensure that it has been successfully executed.
--reap

connection $con_aux1;
unlock tables;

connection default;
}
if (!$success)
{
--echo Error: Wasn't able to run '$statement' with FTWRL active in another connection!
unlock tables;
connection default;
--reap
}

if ($cleanup_stmt)
{
--eval $cleanup_stmt;
}

if (!$skip_3rd_check)
{
#
# Finally, let us check that FTWRL will succeed if this statement
# is active but has already closed its tables.
#
connection default;
set debug_sync='execute_command_after_close_tables SIGNAL parked WAIT_FOR go';
--send_eval $statement;

connection $con_aux1;
set debug_sync="now WAIT_FOR parked";
--send flush tables with read lock

connection $con_aux2;
--enable_result_log
--enable_query_log
let $wait_condition=
  select count(*) = 0 from information_schema.processlist
  where info = "flush tables with read lock";
--source include/wait_condition.inc
--disable_result_log
--disable_query_log

if ($success)
{
--echo Success: Was able to run FTWRL while '$statement' was active in another connection.
connection $con_aux1;
# Apparently FTWRL was successfully executed and so was not blocked by
# the statement being checked. To be safe against wait_condition.inc
# succeeding due to races let us first reap the FTWRL to ensure that it
# has been successfully executed.
--reap
unlock tables;
set debug_sync="now SIGNAL go";
connection default;
--reap
}
if (!$success)
{
--echo Error: Wasn't able to run FTWRL while '$statement' was active in another connection!
set debug_sync="now SIGNAL go";
connection default;
--reap
connection $con_aux1;
--reap
unlock tables;
connection default;
}

set debug_sync= "RESET";
if ($cleanup_stmt)
{
--eval $cleanup_stmt;
}

}

--enable_result_log
--enable_query_log
