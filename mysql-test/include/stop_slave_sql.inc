# ==== Purpose ====
#
# Issues STOP SLAVE SQL_THREAD on the current connection. Then waits
# until the SQL thread has stopped, or until a timeout is reached.
#
# Please use this instead of 'STOP SLAVE SQL_THREAD', to reduce the
# risk of races in test cases.
#
# This will fail if the SQL thread has an error.
#
#
# ==== Usage ====
#
# [--let $rpl_channel_name= NAME]
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/stop_slave_sql.inc
#
# Parameters:
#   $rpl_channel_name
#     If this is not '', a FOR CHANNEL  '$rpl_channel_name' is added.
#
#   $slave_timeout
#     See include/wait_for_slave_param.inc
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= stop_slave_sql.inc
--source include/begin_include_file.inc


if (!$rpl_debug)
{
  --disable_query_log
}


if ($rpl_channel_name != '')
{
  eval STOP SLAVE SQL_THREAD FOR CHANNEL  '$rpl_channel_name';
}
if ($rpl_channel_name == '')
{
  STOP SLAVE SQL_THREAD;
}

--source include/wait_for_slave_sql_to_stop.inc


--let $include_filename= stop_slave_sql.inc
--source include/end_include_file.inc
