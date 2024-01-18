# ==== Purpose ====
#
# Shut down replication initialized by include/rpl/init.inc.
#
# This syncs all servers, executes STOP REPLICA on all servers, executes
# CHANGE REPLICATION SOURCE on all servers, and disconnects all connections
# configured by rpl/init.inc.
#
# It does not execute RESET BINARY LOGS AND GTIDS or RESET REPLICA, because that would
# remove binlogs which are possibly useful debug information in case
# the test case later fails with a result mismatch.  If you need that,
# source include/rpl/reset.inc before you source this file.
#
#
# ==== Usage ====
#
# [--let $rpl_skip_sync = 1]
# [--let $rpl_skip_delete_channels = 1]
# [--let $rpl_skip_stop_slave = 1]
# [--let $rpl_only_running_threads = 1]
# [--let $rpl_debug = 1]
# [--let $rpl_group_replication = 1]
# [--let $rpl_gtid_utils = 1]
# [--let $rpl_assert_transaction_length = 1]
# --source include/rpl/deinit.inc
#
# Parameters:
#   $rpl_skip_sync
#     By default, all slaves are synced using rpl/sync.inc. Set this
#     option to 1 to disable this behavior (note that you must
#     manually sync all servers in this case). Normally you want to
#     sync, but you need to disable sync if you use multi-source.
#
#  $rpl_skip_delete_channels
#     In multi-source repliation, to restore the SHOW REPLICA STATUS state,
#     it is necessary to delete the channels that are created. By default,
#     channels are deleted in multi-source. set it to 1, if channel deletion
#     is not required.
#
#  $rpl_skip_stop_slave
#     If set to '1' the slaves are not stopped. Useful if the slaves are stopped
#     in the test case before sourcing this file.
#
#   $rpl_only_running_threads
#     If one or both of the IO and SQL threads is stopped, sync and
#     stop only the threads that are running.  See
#     include/rpl/sync.inc and include/rpl/stop_replica.inc for details.
#
#   $rpl_debug
#     See include/rpl/init.inc
#
#   $rpl_group_replication
#     This checks for whether we have Group Replication enabled or not. Based
#     on the check this parameter we execute CHANGE REPLICATION SOURCE or not in this
#     inc file.
#
#   $rpl_gtid_utils
#     Source include/rpl/gtid_utils_end.inc on every server.
#
#   $rpl_assert_transaction_length
#     Source include/rpl/assert_transaction_length_all_logs.inc on every server.
#
# Note:
#   This script will fail if Last_SQL_Error or Last_IO_Error is
#   nonempty.  If you expect an error in the SQL thread, you should
#   normally call this script as follows:
#
#     --source include/rpl/wait_for_applier_error.inc
#     --source include/rpl/stop_receiver.inc
#     RESET REPLICA;
#     --let $rpl_only_running_threads= 1
#     --source include/rpl/deinit.inc
#
#
# ==== Side effects ====
#
# Changes the current connection to 'default'.


--let $include_filename= rpl/deinit.inc
--source include/begin_include_file.inc


if (!$rpl_inited)
{
  --die ERROR IN TEST: rpl/deinit.inc was sourced when replication was not configured. Most likely, rpl/deinit.inc was sourced twice or rpl/init.inc has not been sourced.
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
  --source include/connection.inc

  # Only check slave threads for error on hosts that were at some
  # point configured as slave.
  --let $_tmp= query_get_value(SHOW REPLICA STATUS, Source_Host, 1)
  if ($_tmp != 'No such row')
  {
    --source include/rpl/assert_replica_no_error.inc
  }

  --dec $_rpl_server
}

if (!$rpl_skip_sync)
{
  --source include/rpl/sync.inc
}

if ($rpl_assert_transaction_length)
{
  --let $skip_output= 1
  --let $_rpl_server= $rpl_server_count
  while ($_rpl_server)
  {
    --let $rpl_connection_name= server_$_rpl_server
    --source include/connection.inc
    --source include/rpl/assert_transaction_length_all_logs.inc
    --dec $_rpl_server
  }
}

if ($rpl_group_replication)
{
  --let $_rpl_server= $rpl_server_count
  while ($_rpl_server)
  {
    --let $rpl_connection_name= server_$_rpl_server
    --source include/connection.inc

    --let $_group_replication_member_state= `SELECT SERVICE_STATE FROM performance_schema.replication_connection_status connection_status WHERE connection_status.group_name="$group_replication_group_name"`
    if ($_group_replication_member_state == 'ON')
    {
      --source include/stop_group_replication.inc
    }
    --source include/rpl/stop_dump_threads.inc
    --dec $_rpl_server
  }
}

if (!$rpl_group_replication)
{
  if (!$rpl_skip_stop_slave)
  {
    --source include/rpl/stop_all_replicas.inc
  }
}

# Clear the configuration of privilege_checks_user account
if (!$rpl_group_replication)
{
  --let $_rpl_server= $rpl_server_count
  while ($_rpl_server)
  {
    if ($rpl_privilege_checks_user != '')
    {
      --let $rpl_server_number= $_rpl_server
      --source include/rpl/get_instance_priv_checks_user.inc
      if ($rpl_instance_privilege_checks_user != NULL)
      {
        --disable_query_log
        --let $rpl_connection_name= server_$_rpl_server
        --source include/connection.inc

        if (!$rpl_multi_source)
        {
          CHANGE REPLICATION SOURCE TO PRIVILEGE_CHECKS_USER=NULL;
        }
        if ($rpl_multi_source)
        {
          if($rpl_channel_name)
          {
            eval CHANGE REPLICATION SOURCE TO PRIVILEGE_CHECKS_USER=NULL FOR CHANNEL $rpl_channel_name;
          }
          if(!$rpl_channel_name)
          {
            CHANGE REPLICATION SOURCE TO PRIVILEGE_CHECKS_USER=NULL FOR CHANNEL '';
          }
        }
        --enable_query_log
        --source include/rpl/clear_priv_checks_user_configuration.inc
      }
    }
    --dec $_rpl_server
  }
  --let $rpl_privilege_checks_user=
  --let $rpl_change_topology_user_created=
}

# Clear replication asynchronous failover configuration tables
if (!$rpl_group_replication)
{
  --let $_rpl_server= $rpl_server_count
  while ($_rpl_server)
  {
    --let $rpl_connection_name= server_$_rpl_server
    --source include/connection.inc

    --source include/rpl/clear_asynchronous_failover_configuration.inc

    --dec $_rpl_server
  }
}

# Restore the server state by deleting all channels
if ($rpl_multi_source)
{
  if (!$rpl_skip_delete_channels)
  {
    --let $rpl_reset_slave_all= 1
    --let $rpl_source_file= include/rpl/impl/reset_replica_helper.inc
    --source include/rpl/for_each_connection.inc
    --let $rpl_reset_slave_all= 0
  }
}

# mtr configures server 2 to be a slave before it runs the test. We
# have to restore that state now, so we change topology to 1->2.

# rpl_change_topology executes CHANGE REPLICATION SOURCE for each connection.
# we restore the gtid state by sourcing rpl_change_topology below

# Restore state for GTID_ONLY

if ($rpl_gtid_only)
{
  --let $_rpl_server= $rpl_server_count
  while ($_rpl_server)
  {
    --let $rpl_connection_name= server_$_rpl_server
    --source include/connection.inc

    --disable_query_log
    if (!$rpl_multi_source)
    {
      CHANGE REPLICATION SOURCE TO GTID_ONLY = 0;
    }
    if ($rpl_multi_source)
    {
      if($rpl_channel_name)
      {
        eval CHANGE REPLICATION SOURCE TO GTID_ONLY = 0 FOR CHANNEL $rpl_channel_name;
      }
      if(!$rpl_channel_name)
      {
        CHANGE REPLICATION SOURCE TO GTID_ONLY = 0 FOR CHANNEL '';
      }
    }
    --enable_query_log
    --dec $_rpl_server
  }
}

--let $use_gtids= 0
--let $rpl_group_replication= 0

--source include/rpl/change_topology.inc

--let $rpl_topology=none
--source include/rpl/change_topology.inc


--connection default
--let $_rpl_server= $rpl_server_count
--let $_rpl_one= _1
while ($_rpl_server)
{
  if ($rpl_gtid_utils)
  {
    # Drop gtid_utils
    --let $rpl_connection_name= server_$_rpl_server
    --source include/connection.inc
    --source include/rpl/gtid_utils_end.inc
    --connection default
  }
  # Stop the Binlog Dump thread
  --let $rpl_connection_name= server_$_rpl_server
  --source include/connection.inc
  --source include/rpl/stop_dump_threads.inc

  # Restore state for REQUIRE_ROW_FORMAT
  if ($rpl_require_row_format)
  {
    --disable_query_log
    if (!$rpl_multi_source)
    {
      CHANGE REPLICATION SOURCE TO REQUIRE_ROW_FORMAT=0;
    }
    if ($rpl_multi_source)
    {
      if($rpl_channel_name)
      {
        eval CHANGE REPLICATION SOURCE TO REQUIRE_ROW_FORMAT = 0 FOR CHANNEL $rpl_channel_name;
      }
      if(!$rpl_channel_name)
      {
        CHANGE REPLICATION SOURCE TO REQUIRE_ROW_FORMAT = 0 FOR CHANNEL '';
      }
    }
    --enable_query_log
  }

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

# cleanup any files created by JSON iterators
--source include/destroy_json_functions.inc

# Do not restore connection, because we have disconnected it.
--let $skip_restore_connection= 1
--let $include_filename= rpl/deinit.inc
--source include/end_include_file.inc
