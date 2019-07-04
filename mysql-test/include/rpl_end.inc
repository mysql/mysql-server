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
# [--let $rpl_group_replication= 1]
# [--let $rpl_gtid_utils= 1]
# [--let $rpl_assert_transaction_length= 1]
# --source include/rpl_end.inc
#
# Parameters:
#   $rpl_skip_sync
#     By default, all slaves are synced using rpl_sync.inc. Set this
#     option to 1 to disable this behavior (note that you must
#     manually sync all servers in this case). Normally you want to
#     sync, but you need to disable sync if you use multi-source.
#
#  $rpl_skip_delete_channels
#     In multi-source repliation, to restore the SHOW SLAVE STATUS state,
#     it is necessary to delete the channels that are created. By default,
#     channels are deleted in multi-source. set it to 1, if channel deletion
#     is not required.
#
#   $rpl_only_running_threads
#     If one or both of the IO and SQL threads is stopped, sync and
#     stop only the threads that are running.  See
#     include/rpl_sync.inc and include/stop_slave.inc for details.
#
#   $rpl_debug
#     See include/rpl_init.inc
#
#   $rpl_group_replication
#     This checks for whether we have Group Replication enabled or not. Based
#     on the check this parameter we execute change master or not in this
#     inc file.
#
#   $rpl_gtid_utils
#     Source include/gtid_utils_end.inc on every server.
#
#   $rpl_assert_transaction_length
#     Source include/assert_transaction_length_all_logs.inc on every server.
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

# By default one shall delete channels to restore the state
if ($rpl_skip_delete_channels == '')
{
  --let $rpl_skip_delete_channels = 0
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

if (!$rpl_skip_sync)
{
  --source include/rpl_sync.inc
}

if ($rpl_assert_transaction_length)
{
  --let $skip_output= 1
  --let $_rpl_server= $rpl_server_count
  while ($_rpl_server)
  {
    --let $rpl_connection_name= server_$_rpl_server
    --source include/rpl_connection.inc
    --source include/assert_transaction_length_all_logs.inc
    --dec $_rpl_server
  }
}

if ($rpl_group_replication)
{
  --let $_rpl_server= $rpl_server_count
  while ($_rpl_server)
  {
    --let $rpl_connection_name= server_$_rpl_server
    --source include/rpl_connection.inc

    --let $_group_replication_member_state= `SELECT SERVICE_STATE FROM performance_schema.replication_connection_status connection_status WHERE connection_status.group_name="$group_replication_group_name"`
    if ($_group_replication_member_state == 'ON')
    {
      --source include/stop_group_replication.inc
    }
    --source include/stop_dump_threads.inc
    --dec $_rpl_server
  }
}

if (!$rpl_group_replication)
{
  --source include/rpl_stop_slaves.inc
}

# Restore the server state by deleting all channels
if ($rpl_multi_source)
{
  if (!$rpl_skip_delete_channels)
  {
    --let $rpl_reset_slave_all= 1
    --let $rpl_source_file= include/rpl_reset_slave_helper.inc
    --source include/rpl_for_each_connection.inc
    --let $rpl_reset_slave_all= 0
  }
}

# mtr configures server 2 to be a slave before it runs the test. We
# have to restore that state now, so we change topology to 1->2.

# rpl_change_topology executes CHANGE MASTER for each connection.
# we restore the gtid state by sourcing rpl_change_topology below
--let $use_gtids= 0
--let $rpl_group_replication= 0

--source include/rpl_change_topology.inc

--let $rpl_topology=none
--source include/rpl_change_topology.inc


--connection default
--let $_rpl_server= $rpl_server_count
--let $_rpl_one= _1
while ($_rpl_server)
{
  if ($rpl_gtid_utils)
  {
    # Drop gtid_utils
    --let $rpl_connection_name= server_$_rpl_server
    --source include/rpl_connection.inc
    --source include/gtid_utils_end.inc
    --connection default
  }
  # Stop the Binlog Dump thread
  --let $rpl_connection_name= server_$_rpl_server
  --source include/rpl_connection.inc
  --source include/stop_dump_threads.inc
  --connection default

  # Unconfigure connections
  --disconnect server_$_rpl_server
  --disconnect server_$_rpl_server$_rpl_one
  --dec $_rpl_server
}

# reset as next test can be any of rpl or group_replication
--let $using_rpl_group_replication_default_connections= 0

--let $rpl_inited= 0
--let $_rpl_server_count=
--let $rpl_multi_source= 0

# Do not restore connection, because we have disconnected it.
--let $skip_restore_connection= 1
--let $include_filename= rpl_end.inc
--source include/end_include_file.inc
