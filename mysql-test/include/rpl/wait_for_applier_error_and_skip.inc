# ==== Purpose ====
#
# Wait for slave SQL error, skip the erroneous statement and restart
# slave
#
# ==== Usage ====
#
# --let $slave_sql_error= NUMBER
# [--let $show_sql_error= 1]
# [--let $slave_skip_counter= 1]
# [--let $not_switch_connection= 1]
# [--let $rpl_channel_name= 'NAME']
# [--let $rpl_debug= 1]
# [--let $slave_timeout= NUMBER]
# --source include/rpl/wait_for_applier_error_and_skip.inc
#
# Parameters:
#
#   $slave_sql_errno
#     The error number to wait for. This is required. (See
#     rpl/wait_for_applier_error.inc)
#
#   $show_sql_error
#     If set, will print the error to the query log.
# 
#   $slave_skip_counter
#     If set, skip this number of events. If not set, skip one event.
#
#   $not_switch_connection
#     By default, this script executes on the connection 'slave'.  If
#     $not_switch_connection is set, this script executes on the current
#     connection.  In any case, the original connection is restored.
#
#   $rpl_channel_name
#     See include/rpl/wait_for_replica_status.inc
#
#   $slave_timeout
#     See include/rpl/wait_for_replica_status.inc
#
#   $rpl_debug
#     See include/rpl/init.inc


--let $include_filename= rpl/wait_for_applier_error_and_skip.inc [errno=$slave_sql_errno]
if ($rpl_channel_name)
{
  --let $include_filename= rpl/wait_for_applier_error_and_skip.inc [errno=$slave_sql_errno FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc


if (!$rpl_debug)
{
  --disable_query_log
}


if (!$not_switch_connection)
{
  --let $rpl_connection_name= slave
  --source include/connection.inc
}


source include/rpl/wait_for_applier_error.inc;


# skip the erroneous statement
if ($slave_skip_counter) {
  eval SET GLOBAL SQL_REPLICA_SKIP_COUNTER= $slave_skip_counter;
}
if (!$slave_skip_counter) {
  SET GLOBAL SQL_REPLICA_SKIP_COUNTER=1;
}
source include/rpl/start_replica.inc;


--let $include_filename= rpl/wait_for_applier_error_and_skip.inc [errno=$slave_sql_errno]
--source include/end_include_file.inc
