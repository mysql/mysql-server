# ==== Purpose ====
#
# Finish a sync-point wait with suppressing an error that was
# previously started by execute_to_sync_point.inc
#
# See also: execute_to_sync_point.inc, execute_at_sync_point.inc
#
# ==== Usage ====
#
# --let $statement_connection= CONNECTION_NAME
# --let $statement= STATEMENT
# --let $sync_point= SYNC_POINT_NAME
# --let $suppress_error_number= N
# [--let $quiet= [0|2]]
# --source include/execute_statement_at_sync_point.inc
#
# Parameters:
#   See execute_at_sync_point.inc
#
#   $suppress_error_number
#     Suppress the error set by $suppress_error_number. Suppress
#     nothing if $suppress_error_number is not set.


--let $include_silent= 1
--let $include_filename= execute_at_sync_point.inc
--source include/begin_include_file.inc
--let $include_silent= 0

# Sanity check.
if ($suppress_error_number == '')
{
  --die !!!ERROR IN TEST: set $suppress_error_number
}
if ($statement_connection == '')
{
  --die !!!ERROR IN TEST: set $statement_connection
}
--let $_esp_auxiliary_connection= $CURRENT_CONNECTION
if ($auxiliary_connection != '')
{
  --let $_esp_auxiliary_connection= $auxiliary_connection
}
if ($statement_connection == $_esp_auxiliary_connection)
{
  --die !!!ERROR IN TEST: set $statement_connection to something else than $auxiliary_connection
}
if ($statement == '')
{
  --die !!!ERROR IN TEST: set $statement
}
if ($sync_point == '')
{
  --die !!!ERROR IN TEST: set $sync_point
}

if (!$rpl_debug)
{
  --disable_query_log
}

# Tell statement to continue on auxiliary connection.
--connection $_esp_auxiliary_connection
eval SET @@SESSION.DEBUG_SYNC = 'now SIGNAL _esp_go_$sync_point$underscore$statement_connection';

# Wait for statement to finish.
if ($_esp_quiet != 2)
{
  --echo [END] $statement;
}
--connection $statement_connection
if (!$skip_reap)
{
  --error $suppress_error_number
  reap;
}
--connection $_esp_auxiliary_connection

--let $include_filename= execute_at_sync_point.inc
--source include/end_include_file.inc
