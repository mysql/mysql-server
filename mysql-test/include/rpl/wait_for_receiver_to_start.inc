# ==== Purpose ====
#
# Waits until the IO thread of the current connection has started and
# connected to the master (i.e., until SHOW REPLICA STATUS returns Yes
# in the Replica_IO_Running field), or until a timeout is reached.
#
#
# ==== Usage ====
#
# [--let $rpl_channel_name= 'NAME']
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# [--let $rpl_allow_error= 1]
# --source include/rpl/wait_for_receiver_to_start.inc
#
# Parameters:
#   $rpl_channel_name
#     See include/rpl/wait_for_replica_status.inc
#
#   $slave_timeout
#     See include/rpl/wait_for_replica_status.inc
#
#   $rpl_allow_error
#     By default, this file fails if there is an error in the IO
#     thread.  However, the IO thread can recover and reconnect after
#     certain errors.  If such an error is expected, can set
#     $rpl_allow_error=1.  This will prevent this file from failing if
#     there is an error in the IO thread.
#
#   $rpl_debug
#     See include/rpl/init.inc


--let $include_filename= rpl/wait_for_receiver_to_start.inc
if ($rpl_channel_name)
{
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc


let $slave_param= Replica_IO_Running;
let $slave_param_value= Yes;
if (!$rpl_allow_error)
{
  --let $slave_error_param= Last_IO_Errno
}
source include/rpl/wait_for_replica_status.inc;
--let $slave_error_param=
--let $rpl_allow_error= 0


--let $include_filename= rpl/wait_for_receiver_to_start.inc
--source include/end_include_file.inc
