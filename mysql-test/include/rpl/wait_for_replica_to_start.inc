# ==== Purpose ====
#
# Waits until both the IO and SQL threads of the current connection
# have started, or until a timeout is reached.
#
#
# ==== Usage ====
#
# [--let $rpl_channel_name= 'NAME']
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/rpl/wait_for_replica_to_start.inc
#
# Parameters:
#   $rpl_channel_name
#     See include/rpl/wait_for_replica_status.inc
#
#   $slave_timeout
#     See include/rpl/wait_for_replica_status.inc
#
#   $rpl_debug
#     See include/rpl/init.inc


--let $include_filename= rpl/wait_for_replica_to_start.inc
if ($rpl_channel_name)
{
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc


--source include/rpl/wait_for_receiver_to_start.inc
--source include/rpl/wait_for_applier_to_start.inc


--let $include_filename= rpl/wait_for_replica_to_start.inc
--source include/end_include_file.inc
