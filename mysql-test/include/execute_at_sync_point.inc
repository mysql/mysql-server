# ==== Purpose ====
#
# Execute a statement on one connection.  When it reaches a given sync
# point: pause, execute another statement on another connection, and
# unpause.
#
# See also: execute_to_sync_point.inc, execute_from_sync_point.inc
#
# ==== Usage ====
#
# --let $statement_connection= CONNECTION_NAME
# --let $statement= STATEMENT
# [--let $auxiliary_statement= STATEMENT | --let $auxiliary_file= FILE]
# --let $sync_point= SYNC_POINT_NAME
# [--let $auxiliary_connection= CONNECTION_NAME]
# [--let $quiet= [0|1|2]]
# [--let $sync_point_timeout= N]
# --source include/execute_at_sync_point.inc
#
# Parameters:
#
#   $statement_connection
#     The connection on which $statement is to be executed.
#     This must be different from $auxiliary_connection.
#
#   $statement
#     The statement that will be paused on the sync point.
#
#   $auxiliary_statement
#     The statement that will be executed on the current connection while
#     $statement is paused on the sync point.
#
#   $auxiliary_file
#     If this is not an empty string, this file will be sourced on the current
#     connection while $statement is paused on the sync point.
#     Exactly one of $auxiliary_file and $auxiliary_statement must be specified.
#
#   $sync_point
#     The name of the sync point (not quoted).
#
#   $auxiliary_connection
#     By default, $auxiliary_statement, as well as some SET DEBUG
#     statements, are executed on the current connection. If this
#     variable is not empty, uses $auxiliary_connection instead.
#
#   $quiet
#     By default (or if $quiet==0), the following is echoed to the log:
#       START $statement;
#       auxiliary_statement or auxiliary_file;
#       result of $auxiliary_statement or source $auxiliary_file
#       END $statement;
#     If you set $quiet=1, the START and END lines are still there,
#     but from the auxiliary_statement only the result is echoed.
#     If you set $quiet=2, all output is suppressed.
#
#   $sync_point_timeout
#     Both the $statement_connection and the current connection will
#     be waiting for a signal. By default, the wait has a timeout
#     specified by --debug-sync-timeout=N.  You can set
#     $sync_point_timeout to use a different timeout. The unit is
#     seconds.
#
#   $skip_reap
#     By default, the script will wait for $statement to ACK to the
#     client (reap).  Set $skip_reap to prevent that from happening.
#     The caller then has the execute reap manually.
#
# Side effects:
#   If $auxiliary_statement is used, then the result of the statement
#   is stored in the variable $result.

--let $include_silent= 1
--let $include_filename= execute_at_sync_point.inc
--source include/begin_include_file.inc
--let $include_silent= 0

if ($auxiliary_statement != '')
{
  if ($auxiliary_file != '')
  {
    --die !!!ERROR IN TEST: cannot use both $auxiliary_file and $auxiliary_statement
  }
}

if ($auxiliary_statement == '')
{
  if ($auxiliary_file == '')
  {
    --die !!!ERROR IN TEST: must use one of $auxiliary_file or $auxiliary_statement
  }
}

--source include/execute_to_sync_point.inc

# execute auxiliary_statement or source auxiliary_file
if (!$quiet)
{
  if ($auxiliary_statement)
  {
    --echo $auxiliary_statement
  }
  if ($auxiliary_file)
  {
    --echo $auxiliary_file
  }
}
--connection $_esp_auxiliary_connection
if ($auxiliary_statement)
{
  --let $result= `$auxiliary_statement`
  if ($quiet != 2)
  {
    --echo $result
  }
}
if ($auxiliary_file)
{
  --source $auxiliary_file
}

--source include/execute_from_sync_point.inc


--let $include_filename= execute_at_sync_point.inc
--source include/end_include_file.inc
