# ==== Purpose ====
#
# Sync all servers in an arbitrary replication topology. This works
# only if the servers have been configured with rpl/init.inc (and
# possibly rpl/change_topology.inc).
#
#
# ==== Usage ====
#
# [--let $rpl_only_running_threads= 1]
# [--let $use_gtids= 1]
# [--let $rpl_debug= 1]
# [--let $slave_timeout= NUMBER]
# --source include/rpl/sync.inc
#
# Parameters:
#   $rpl_only_running_threads
#     By default, this script assumes that both the IO thread and the
#     SQL thread are running and fails if one of them is stopped.  If
#     $rpl_only_running_threads is set, this script first checks
#     which slave threads are running:
#      - If both threads are running, sync both threads with master.
#      - If only IO thread is running, sync IO thread with master.
#      - If only SQL thread is running, sync SQL thread with IO thread.
#      - If no thread is running, don't sync.
#
#   $use_gtids
#     If set, uses GTIDs instead of filename and offset for positions.
#
#   $slave_timeout
#     Set the timeout when waiting for threads to sync. See
#     include/rpl/wait_for_replica_status.inc
#
#   $rpl_debug
#     See include/rpl/init.inc
#
#
# ==== Side effects ====
#
# Does not change the current connection (note that this is different
# from mysqltest's built-in sync_slave_with_master command).


--let $include_filename= rpl/sync.inc
--source include/begin_include_file.inc

#
#@TODO: Ideally, just being in slave connection, we should have a way to sync
# all the masters in one command, like below
# --connection slave
# --source include/rpl/sync.inc
# For now, we disable rpl/sync.inc when multisource is used.
#

if ($rpl_multi_source)
{
  --die ERROR IN TEST: rpl/sync.inc cannot be used when $rpl_multi_source=1. If you use multi-source, you must manually sync all servers using rpl/sync_to_replica.inc or equivalent.
}

# Compute $rpl_sync_chain if needed.  We could have done this in
# rpl/change_topology.inc, but instead we do it here because that
# means we only compute $rpl_sync_chain when it is needed.
if ($rpl_sync_chain_dirty)
{
  --source include/rpl/impl/generate_sync_chain.inc
  --let $rpl_sync_chain_dirty= 0
}


if ($rpl_debug)
{
  --echo \$rpl_sync_chain = '$rpl_sync_chain' \$rpl_only_running_threads= $rpl_only_running_threads
}

if (!$rpl_group_replication)
{
  if (!$rpl_server_count_length)
  {
    --die \$rpl_server_count_length is not set. Did you call rpl/init.inc?
  }
}

--let $_rpl_i= 1
--let $_rpl_connect= 0
if(!$rpl_group_replication)
{
  while ($_rpl_i) {
    # $rpl_sync_chain consists of a sequence of sync chains. Each sync
    # chain has the form:
    #
    #   <space><server1_1><server1_2>...<server1_N>
    #
    # So the space character indicates that a new sync chain starts.
    --let $_rpl_server= `SELECT TRIM(SUBSTR('$rpl_sync_chain', 1 + ($_rpl_i - 1) * $rpl_server_count_length, $rpl_server_count_length))`

    if ($_rpl_server)
    {
      if ($rpl_debug)
      {
        --echo [sync server_$_rpl_prev_server -> server_$_rpl_server]
      }
      if ($rpl_only_running_threads)
      {
        --connection server_$_rpl_server
        --let $_rpl_replica_io_running= query_get_value(SHOW REPLICA STATUS, Replica_IO_Running, 1)
        --let $_rpl_replica_sql_running= query_get_value(SHOW REPLICA STATUS, Replica_SQL_Running, 1)
        if ($rpl_debug)
        {
          --echo Sync IO: $_rpl_replica_io_running; Sync SQL: $_rpl_replica_sql_running
        }
        if ($_rpl_replica_io_running != No)
        {
          --connection server_$_rpl_prev_server
          --let $sync_slave_connection= server_$_rpl_server
          if ($_rpl_replica_sql_running != No)
          {
            --source include/rpl/sync_to_replica.inc
          }
          if ($_rpl_replica_sql_running == No)
          {
            --source include/rpl/sync_to_replica_received.inc
          }
        }
        if ($_rpl_replica_io_running == No)
        {
          if ($_rpl_replica_sql_running != No)
          {
            --source include/rpl/sync_with_recieved.inc
          }
        }
      }
      if (!$rpl_only_running_threads)
      {
        --connection server_$_rpl_prev_server
        --let $saved_value= $sync_slave_connection
        --let $sync_slave_connection= server_$_rpl_server
        --source include/rpl/sync_to_replica.inc
        --let $sync_slave_connection= $saved_value
      }
    }

  # This happens at the beginning of a new sync subchain and at the
  # end of the full sync chain.
    if (!$_rpl_server)
    {
      --inc $_rpl_i
      --let $_rpl_server= `SELECT TRIM(SUBSTR('$rpl_sync_chain', 1 + ($_rpl_i - 1) * $rpl_server_count_length, $rpl_server_count_length))`

      if (!$_rpl_server)
      {
        # terminate loop
        --let $_rpl_i= -1
      }
    }

    --let $_rpl_prev_server= $_rpl_server
    --inc $_rpl_i
  }
}

if ($rpl_group_replication)
{
  --let $_rpl_old_con= $CURRENT_CONNECTION
  --let $_rpl_server= $rpl_server_count

  while ($_rpl_server)
  {
    --let $rpl_connection_name= server_$_rpl_server
    --source include/connection.inc

    if ($biggest_gtid_executed == '')
    {
      --let $biggest_gtid_executed= `SELECT @@GLOBAL.GTID_EXECUTED`
      --let $biggest_gtid_executed_server= server_$_rpl_server
    }

    --let $gtid_executed= `SELECT @@GLOBAL.GTID_EXECUTED`
    --let $diff_between_gtid= `SELECT GTID_SUBTRACT('$gtid_executed', '$biggest_gtid_executed')`
    if ($diff_between_gtid != '')
    {
      --let $biggest_gtid_executed= $gtid_executed
      --let $biggest_gtid_executed_server= server_$_rpl_server
    }
    --dec $_rpl_server
  }

  --let $_rpl_server= $rpl_server_count
  while ($_rpl_server)
  {
    --let $rpl_connection_name=server_$_rpl_server
    --source include/connection.inc
    --let $_member_id= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)
    --let $_member_state= `SELECT member_state FROM performance_schema.replication_group_members group_members WHERE group_members.channel_name="group_replication_applier" AND group_members.member_id="$_member_id"`
    if ($_member_state == "ONLINE")
    {
      --let $rpl_connection_name= $biggest_gtid_executed_server
      --source include/connection.inc
      --let $sync_slave_connection= server_$_rpl_server
      if ($sync_slave_connection != $biggest_gtid_executed_server)
      {
        --source include/rpl/sync_to_replica.inc
      }
    }
    --dec $_rpl_server
  }
  --connection $_rpl_old_con
}

--let $include_filename= rpl/sync.inc
--source include/end_include_file.inc
