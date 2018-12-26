# ==== Purpose ====
#
# Begin to execute a statement, stop at a given sync point.
#
# See also: execute_from_sync_point.inc, execute_at_sync_point.inc
#
# ==== Usage ====
#
# --let $statement_connection= CONNECTION_NAME
# --let $statement= STATEMENT
# --let $sync_point= SYNC_POINT_NAME
# [--let $auxiliary_connection= CONNECTION_NAME]
# [--let $quiet= [0|2]]
# [--let $sync_point_timeout= N]
# --source include/execute_to_sync_point.inc
#
# Parameters:
#   See execute_at_sync_point.inc.

--let $include_silent= 1
--let $include_filename= execute_to_sync_point.inc
--source include/begin_include_file.inc
--let $include_silent= 0

if (!$rpl_debug)
{
  --disable_query_log
}

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
  --echo statement_connection=$statement_connection
  --echo auxiliary_connection=$_esp_auxiliary_connection
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

--let $_esp_timeout=
if ($sync_point_timeout)
{
  --let $_esp_timeout= TIMEOUT $sync_point_timeout
}

# On $statement_connection, setup sync point and begin execute statement.
--let $rpl_connection_name= $statement_connection
--source include/rpl_connection.inc

eval SET @@SESSION.DEBUG_SYNC = '$sync_point SIGNAL _esp_parked_$sync_point$underscore$statement_connection WAIT_FOR _esp_go_$sync_point$underscore$statement_connection $_esp_timeout';

if ($quiet != 2)
{
  --echo [START] $statement;
}

--send
eval $statement;

# On auxiliary connection, begin waiting for statement to reach the sync point.
if ($rpl_debug)
{
  --echo waiting for debug sync point _esp_parked_$sync_point$underscore$statement_connection $_esp_timeout
}
--let $rpl_connection_name= $_esp_auxiliary_connection
--source include/rpl_connection.inc

eval SET @@SESSION.DEBUG_SYNC = 'now WAIT_FOR _esp_parked_$sync_point$underscore$statement_connection $_esp_timeout';

--let $skip_restore_connection= 1
--let $include_filename= execute_to_sync_point.inc
--source include/end_include_file.inc
