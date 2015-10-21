# ==== Purpose ====
#
# Execute a statement on one connection specified by $statement_connection,
# stop at a given sync point. Then kill the statement from another
# connection specified by $auxiliary_connection.
#
# See also: execute_to_sync_point.inc
#
# ==== Usage ====
#
# --let $statement_connection= CONNECTION_NAME
# --let $statement= STATEMENT
# --let $sync_point= SYNC_POINT_NAME
# --let $auxiliary_connection= CONNECTION_NAME
# [--let $quiet= [0|2]]
# [--let $sync_point_timeout= N]
# --source include/kill_at_sync_point.inc
#
# Parameters:
#   See execute_at_sync_point.inc.

--let $include_silent= 1
--let $include_filename= kill_at_sync_point.inc
--source include/begin_include_file.inc
--let $include_silent= 0

# Sanity check.
if ($statement_connection == '')
{
  --die !!!ERROR IN TEST: set $statement_connection
}
if ($auxiliary_connection == '')
{
  --die !!!ERROR IN TEST: set $auxiliary_connection
}

--connection $statement_connection
--let $ID= `select connection_id()`

--source include/execute_to_sync_point.inc

--connection $auxiliary_connection
--disable_query_log
--eval kill query $ID
--enable_query_log
--connection $statement_connection
--error ER_QUERY_INTERRUPTED
--reap

--connection $auxiliary_connection
--let $include_filename= kill_at_sync_point.inc
--source include/end_include_file.inc
