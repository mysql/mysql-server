# ==== Purpose ====
#
# Finish a sync-point wait that was previously started by
# execute_to_sync_point.inc
#
# See also: execute_to_sync_point.inc, execute_at_sync_point.inc
#
# ==== Usage ====
#
# --let $statement_connection= CONNECTION_NAME
# --let $statement= STATEMENT
# --let $sync_point= SYNC_POINT_NAME
# [--let $auxiliary_connection= CONNECTION_NAME]
# [--let $quiet= [0|2]]
# --source include/execute_from_sync_point.inc
#
# Parameters:
#   See execute_at_sync_point.inc


--let $include_silent= 1
--let $include_filename= execute_from_sync_point.inc
--source include/begin_include_file.inc
--let $include_silent= 0

# Sanity check.
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
if ($rpl_debug)
{
  --echo statement_connection=$statement_connection auxiliary_connection=$auxiliary_connection statement=$statement sync_point=$sync_point
}

# Tell statement to continue on auxiliary connection.
--let $rpl_connection_name= $_esp_auxiliary_connection
--source include/connection.inc
eval SET @@SESSION.DEBUG_SYNC = 'now SIGNAL _esp_go_$sync_point$underscore$statement_connection';

# Wait for statement to finish.
--let $rpl_connection_name= $statement_connection
--source include/connection.inc
if ($_esp_quiet != 2)
{
  --echo [END] $statement;
}
if (!$skip_reap)
{
  reap;
}
--let $rpl_connection_name= $_esp_auxiliary_connection
--source include/connection.inc

--let $include_filename= execute_from_sync_point.inc
--source include/end_include_file.inc
