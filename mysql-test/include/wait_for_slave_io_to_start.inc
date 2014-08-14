# ==== Purpose ====
#
# Waits until the IO thread of the current connection has started and
# connected to the master (i.e., until SHOW SLAVE STATUS returns Yes
# in the Slave_IO_Running field), or until a timeout is reached.
#
#
# ==== Usage ====
#
# [--let $rpl_channel_name= NAME]
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# [--let $rpl_allow_error= 1]
# --source include/wait_for_slave_io_to_start.inc
#
# Parameters:
#   $rpl_channel_name
#     See include/wait_for_slave_param.inc
#
#   $slave_timeout
#     See include/wait_for_slave_param.inc
#
#   $rpl_allow_error
#     By default, this file fails if there is an error in the IO
#     thread.  However, the IO thread can recover and reconnect after
#     certain errors.  If such an error is expected, can set
#     $rpl_allow_error=1.  This will prevent this file from failing if
#     there is an error in the IO thread.
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= wait_for_slave_io_to_start.inc
--source include/begin_include_file.inc


let $slave_param= Slave_IO_Running;
let $slave_param_value= Yes;
if (!$rpl_allow_error)
{
  --let $slave_error_param= Last_IO_Errno
}
source include/wait_for_slave_param.inc;
--let $slave_error_param=
--let $rpl_allow_error= 0


--let $include_filename= wait_for_slave_io_to_start.inc
--source include/end_include_file.inc
