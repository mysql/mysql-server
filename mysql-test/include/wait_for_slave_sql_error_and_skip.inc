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
# --source include/wait_for_slave_sql_error_and_skip.inc
#
# Parameters:
#
#   $slave_sql_errno
#     The error number to wait for. This is required. (See
#     wait_for_slave_sql_error.inc)
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
#     See include/wait_for_slave_param.inc
#
#   $slave_timeout
#     See include/wait_for_slave_param.inc
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= wait_for_slave_sql_error_and_skip.inc [errno=$slave_sql_errno]
if ($rpl_channel_name)
{
  --let $include_filename= wait_for_slave_sql_error_and_skip.inc [errno=$slave_sql_errno FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc


if (!$rpl_debug)
{
  --disable_query_log
}


if (!$not_switch_connection)
{
  --let $rpl_connection_name= slave
  --source include/rpl_connection.inc
}


source include/wait_for_slave_sql_error.inc;


# skip the erroneous statement
if ($slave_skip_counter) {
  eval SET GLOBAL SQL_REPLICA_SKIP_COUNTER= $slave_skip_counter;
}
if (!$slave_skip_counter) {
  SET GLOBAL SQL_REPLICA_SKIP_COUNTER=1;
}
source include/start_slave.inc;


--let $include_filename= wait_for_slave_sql_error_and_skip.inc [errno=$slave_sql_errno]
--source include/end_include_file.inc
