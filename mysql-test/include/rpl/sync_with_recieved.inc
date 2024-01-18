# ==== Purpose ====
#
# Sync the slave SQL thread with the IO thread on the current connection.
#
# ==== Usage ====
#
# [--let $use_gtids= 1]
# [--let $rpl_channel_name= 'NAME']
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/rpl/sync_with_recieved.inc
#
# Parameters:
#
#   $use_gtids
#     If set, uses GTIDs instead of filename and offset for positions.
#
#   $rpl_channel_name
#     See include/rpl/wait_for_replica_status.inc
#
#   $slave_timeout
#     By default, the synchronization timeouts after 300 seconds. If
#     $slave_timeout is set, the synchronization timeouts after
#     $slave_timeout seconds.
#
#   $rpl_debug
#     See include/rpl/init.inc


--let $include_filename= rpl/sync_with_recieved.inc
if ($rpl_channel_name)
{
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc

--source include/rpl/save_received_position.inc
--source include/rpl/sync_with_saved.inc
--let $_saved_channel_name=

--let $include_filename= rpl/sync_with_recieved.inc
--source include/end_include_file.inc
