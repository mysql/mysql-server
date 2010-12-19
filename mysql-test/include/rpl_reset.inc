# ==== Purpose ====
#
# Reset all replication servers to a clean state:
#
#  - sync all slaves,
#  - stop all slaves (STOP SLAVE),
#  - remove all binlogs and relay logs (RESET MASTER and RESET SLAVE),
#  - start all slaves again (START SLAVE).
#
# It does not execute CHANGE MASTER, so the replication topology is
# kept intact.
#
#
# ==== Usage ====
#
# [--let $rpl_only_running_threads= 1]
# [--let $rpl_debug= 1]
# [--let $slave_timeout= NUMBER]
# --source include/rpl_end.inc
#
# Parameters:
#   $rpl_only_running_threads
#     If one or both of the IO and SQL threads is stopped, sync and
#     stop only the threads that are running.  See
#     include/rpl_sync.inc and include/stop_slave.inc for details.
#
#   $rpl_debug
#     See include/rpl_init.inc
#
#   $slave_timeout
#     Set the timeout when waiting for slave threads to stop and
#     start, respectively.  See include/wait_for_slave_param.inc
#
# Note:
#   This script will fail if Last_SQL_Error or Last_IO_Error is
#   nonempty.  If you expect an error in the SQL thread, you should
#   normally do this before you source include/rpl_reset.inc:
#
#     --source include/wait_for_slave_sql_error.inc
#     --source include/stop_slave_io.inc
#     RESET SLAVE;

--let $include_filename= rpl_reset.inc
--source include/begin_include_file.inc


if (!$rpl_debug)
{
  --disable_query_log
}


--source include/rpl_sync.inc


if ($rpl_debug)
{
  --echo ---- Stop and reset all servers ----
}
--let $_rpl_server= $rpl_server_count
while ($_rpl_server)
{
  --let $rpl_connection_name= server_$_rpl_server
  --source include/rpl_connection.inc

  # Check if this server is configured to have a master
  if (`SELECT SUBSTRING('$rpl_master_list', 1 + ($_rpl_server - 1) * $rpl_server_count_length, $rpl_server_count_length) != ''`)
  {
    --source include/stop_slave.inc
    RESET SLAVE;
  }
  RESET MASTER;
  --dec $_rpl_server
}


--source include/rpl_start_slaves.inc


--let $include_filename= rpl_reset.inc
--source include/end_include_file.inc
