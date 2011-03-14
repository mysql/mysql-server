#
# SUMMARY
#   Check that a statement is incompatible with FLUSH TABLES WITH READ LOCK.
#
# PARAMETERS 
#   $con_aux1      Name of the 1st aux connection to be used by this script.
#   $con_aux2      Name of the 2nd aux connection to be used by this script.
#   $statement     The statement to be checked.
#   $cleanup_stmt1 The 1st statement to be run in order to revert effects
#                  of statement to be checked.
#   $cleanup_stmt2 The 2nd statement to be run in order to revert effects
#                  of statement to be checked.
#   $skip_3rd_chk  Skip the 3rd stage of checking. The purpose of the third
#                  stage is to check that metadata locks taken by this
#                  statement are incompatible with metadata locks taken
#                  by FTWRL.
#
# EXAMPLE
#   flush_read_lock.test
#
--disable_result_log
--disable_query_log

# Reset DEBUG_SYNC facility for safety.
set debug_sync= "RESET";

#
# First, check that the statement cannot be run under FTWRL.
#
flush tables with read lock;
--disable_abort_on_error
--eval $statement
--enable_abort_on_error
let $err= $mysql_errno;
if ($err)
{
--echo Success: Was not able to run '$statement' under FTWRL.
unlock tables;
}
if (!$err)
{
--echo Error: Was able to run '$statement' under FTWRL!
unlock tables;
if ($cleanup_stmt1)
{
--eval $cleanup_stmt1;
}
if ($cleanup_stmt2)
{
--eval $cleanup_stmt2;
}
}


#
# Then check that this statement is blocked by FTWRL
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
  select count(*) = 1 from information_schema.processlist
  where (state = "Waiting for global read lock" or
         state = "Waiting for commit lock") and
        info = "$statement";
--source include/wait_condition.inc
--disable_result_log
--disable_query_log

if ($success)
{
--echo Success: '$statement' is blocked by FTWRL active in another connection.
}
if (!$success)
{
--echo Error: '$statement' wasn't blocked by FTWRL active in another connection!
}
unlock tables;

connection default;
--reap

if ($cleanup_stmt1)
{
--eval $cleanup_stmt1;
}
if ($cleanup_stmt2)
{
--eval $cleanup_stmt2;
}

if (!$skip_3rd_check)
{
#
# Finally, let us check that FTWRL will not succeed if this
# statement is active but has already closed its tables.
#
connection default;
--eval set debug_sync='execute_command_after_close_tables SIGNAL parked WAIT_FOR go';
--send_eval $statement;

connection $con_aux1;
set debug_sync="now WAIT_FOR parked";
--send flush tables with read lock

connection $con_aux2;
--enable_result_log
--enable_query_log
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where (state = "Waiting for global read lock" or
         state = "Waiting for commit lock") and
        info = "flush tables with read lock";
--source include/wait_condition.inc
--disable_result_log
--disable_query_log

if ($success)
{
--echo Success: FTWRL is blocked when '$statement' is active in another connection.
}
if (!$success)
{
--echo Error: FTWRL isn't blocked when '$statement' is active in another connection!
}
set debug_sync="now SIGNAL go";
connection default;
--reap
connection $con_aux1;
--reap
unlock tables;
connection default;

set debug_sync= "RESET";

if ($cleanup_stmt1)
{
--eval $cleanup_stmt1;
}
if ($cleanup_stmt2)
{
--eval $cleanup_stmt2;
}
}

--enable_result_log
--enable_query_log
