# ==== Purpose ====
#
# Execute a statement on one connection.  When it reaches a given sync
# point: pause, execute another statement on another connection, and
# unpause.
#
# ==== Usage ====
#
# [--let $main_connection= CONNECTION_NAME]
# --let $main_statement= STATEMENT
# --let $middle_connection= CONNECTION_NAME
# --let $middle_statement= STATEMENT
# --let $sync_point= SYNC_POINT_NAME
# [--let $quiet= [0|1|2]]
# [--let $sync_point_timeout= N]
# --source include/execute_statement_at_sync_point.inc
#
# Parameters:
#
#   $main_connection
#     The connection on which the "main" statement is to be executed.
#     If this is not specified, uses the current connection.
#
#   $main_statement
#     The statement that will be paused on the sync point.
#
#   $middle_connection
#     The connection on which the "interluding" statement is to be
#     executed while the main statement is paused on the sync point.
#
#   $middle_statement
#     The statement that will be executed on $middle_connection while
#     $main_statement is paused on the sync point.
#
#   $sync_point
#     The name of the sync point (not quoted).
#
#   $quiet
#     By default (or if $quiet==0), the following is echoed to the log:
#       START $main_statement;
#       $middle_statement;
#       END $main_statement;
#     If you set $quiet=1, the START and END lines are still there,
#     but from the middle statement only the result is echoed.
#     If you set $quiet=2, all output is suppressed.
#
#   $sync_point_timeout
#     Both the main connection and the middle connection will be
#     waiting for a signal. By default, the wait has a timeout
#     specified by --debug-sync-timeout=N.  You can set
#     $sync_point_timeout to use a different timeout. The unit is
#     seconds.

--let $_easp_query_log_enabled= $ENABLED_QUERY_LOG
--disable_query_log

# Configure.
--let $_easp_previous_connection = $CURRENT_CONNECTION

--let $_easp_main_connection= $main_connection
if ($main_connection == '')
{
  --let $_easp_main_connection= $CURRENT_CONNECTION
}

--let $_easp_timeout=
if ($sync_point_timeout)
{
  --let $_easp_timeout= TIMEOUT $sync_point_timeout
}

--let $_easp_quiet= $quiet
if ($quiet == '')
{
  --let $_easp_quiet= 0
}

# Begin waiting for signal on middle connection.
--connection $middle_connection
--send
eval SET @@SESSION.DEBUG_SYNC = 'now WAIT_FOR _easp_parked $_easp_timeout';

# Setup sync point and begin execute main statement.
--connection $_easp_main_connection
eval SET @@SESSION.DEBUG_SYNC = '$sync_point SIGNAL _easp_parked WAIT_FOR _easp_go $_easp_timeout';

if ($_easp_quiet < 2)
{
  --echo [START] $main_statement;
}
--send
eval $main_statement;

# Finish waiting for signal on middle connection, execute middle
# statement, tell main statement to continue.
--connection $middle_connection
reap;
if ($_easp_quiet == 0)
{
  --echo $middle_statement;
}
if ($_easp_quiet == 1)
{
  --let $_easp_result= `$middle_statement`
  --echo $_easp_result
}
if ($_easp_quiet != 1)
{
  eval $middle_statement;
}
SET @@SESSION.DEBUG_SYNC = 'now SIGNAL _easp_go';

# Wait for main statement to finish.
if ($_easp_quiet < 2)
{
  --echo [END] $main_statement;
}
--connection $_easp_main_connection
reap;

# Restore state
--connection $_easp_previous_connection
if ($_easp_query_log_enabled)
{
  --enable_query_log
}
