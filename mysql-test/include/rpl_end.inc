# ==== Purpose ====
#
# Shut down replication initialized by include/rpl_init.inc.
#
# This syncs all servers, executes STOP SLAVE on all servers, executes
# CHANGE MASTER on all servers, and disconnects all connections
# configured by rpl_init.inc.
#
# It does not execute RESET MASTER or RESET SLAVE, because that would
# remove binlogs which are possibly useful debug information in case
# the test case later fails with a result mismatch.  If you need that,
# source include/rpl_reset.inc before you source this file.
#
#
# ==== Usage ====
#
# [--let $rpl_only_running_threads= 1]
# [--let $rpl_debug= 1]
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
# Note:
#   This script will fail if Last_SQL_Error or Last_IO_Error is
#   nonempty.  If you expect an error in the SQL thread, you should
#   normally call this script as follows:
#
#     --source include/wait_for_slave_sql_error.inc
#     --source include/stop_slave_io.inc
#     RESET SLAVE;
#     --let $rpl_only_running_threads= 1
#     --source include/rpl_end.inc
#
#
# ==== Side effects ====
#
# Changes the current connection to 'default'.


--let $include_filename= rpl_end.inc
--source include/begin_include_file.inc


if (!$rpl_inited)
{
  --die ERROR IN TEST: rpl_end.inc was sourced when replication was not configured. Most likely, rpl_end.inc was sourced twice or rpl_init.inc has not been sourced.
}


if ($rpl_debug)
{
  --echo ---- Check that no slave thread has an error ----
}

--let $_rpl_server= $rpl_server_count
while ($_rpl_server)
{
  --let $rpl_connection_name= server_$_rpl_server
  --source include/rpl_connection.inc

  # Only check slave threads for error on hosts that were at some
  # point configured as slave.
  --let $_tmp= query_get_value(SHOW SLAVE STATUS, Master_Host, 1)
  if ($_tmp != 'No such row')
  {
    --source include/check_slave_no_error.inc
  }

  --dec $_rpl_server
}

--source include/rpl_sync.inc
--source include/rpl_stop_slaves.inc

# mtr configures server 2 to be a slave before it runs the test. We
# have to restore that state now, so we change topology to 1->2.
--let $rpl_topology= none
--source include/rpl_change_topology.inc


--connection default
--let $_rpl_server= $rpl_server_count
--let $_rpl_one= _1
while ($_rpl_server)
{
  --disconnect server_$_rpl_server
  --disconnect server_$_rpl_server$_rpl_one
  --dec $_rpl_server
}

--let $rpl_inited= 0

# Do not restore connection, because we have disconnected it.
--let $skip_restore_connection= 1
--let $include_filename= rpl_end.inc
--source include/end_include_file.inc
