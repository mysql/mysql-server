# ==== Purpose ====
#
# Waits until the slave IO thread has been synced, i.e., all events
# have been copied over to slave.  Does not care if the SQL thread is
# in sync (or even running).
#
#
# ==== Usage ====
#
# [--let $sync_slave_connection= <connection_name>]
# [--let $use_gtids= 1]
# [--let $rpl_channel_name= 'NAME']
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/rpl/sync_to_replica_received.inc
#
# Must be called on the master. Will change connection to the slave.
#
# Parameters:
#
#   $use_gtids
#     If set, uses GTIDs instead of filename and offset for positions.
#
#   $sync_slave_connection
#     By default, this script switches connection to 'slave'. If
#     $sync_slave_connection is set, then '$sync_slave_connection' is
#     used instead of 'slave'.
#
#   $slave_timeout
#      See include/rpl/wait_for_replica_status.inc.
#
#   $rpl_debug
#      See include/rpl/init.inc
#
#   $rpl_channel_name
#     See include/rpl/wait_for_replica_status.inc

--let $include_filename= rpl/sync_to_replica_received.inc
if ($rpl_channel_name)
{
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc


--source include/rpl/save_server_position.inc

--let $rpl_connection_name= slave
if ($sync_slave_connection)
{
  --let $rpl_connection_name= $sync_slave_connection
}

# Assert that source and destination connections are not the same.
if ($rpl_connection_name == $CURRENT_CONNECTION)
{
  --echo Trying to sync '$CURRENT_CONNECTION' with '$rpl_connection_name' which are the same server!
  --echo Please double check your test case.
  --die
}

--source include/connection.inc

--source include/rpl/sync_received_with_saved.inc


--let $include_filename= rpl/sync_to_replica_received.inc
--let $skip_restore_connection= 1
--source include/end_include_file.inc
