# ==== Purpose ====
#
# Waits until the IO thread of the current connection has stopped
# gracefully.
#
# Note: this script will fail if the IO thread stops with an error.
# If you expect an error in the IO thread, use
# include/rpl/wait_for_receiver_error.inc instead.
#
# This script also fails if a timeout is reached (default 300
# seconds).
#
#
# ==== Usage ====
#
# [--let $rpl_channel_name= 'NAME']
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/rpl/wait_for_receiver_to_stop.inc
#
# Parameters:
#   $rpl_channel_name
#     See include/rpl/wait_for_replica_status.inc
#
#   $slave_timeout
#     See include/rpl/wait_for_replica_status.inc.
#
#   $rpl_debug
#     See include/rpl/init.inc


--let $include_filename= rpl/wait_for_receiver_to_stop.inc
if ($rpl_channel_name)
{
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc


--let $slave_param= Replica_IO_Running
--let $slave_param_value= No
--let $slave_error_param= Last_IO_Errno
--source include/rpl/wait_for_replica_status.inc
--let $slave_error_param=


--let $include_filename= rpl/wait_for_receiver_to_stop.inc
--source include/end_include_file.inc
