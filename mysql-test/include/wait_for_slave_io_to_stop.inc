# ==== Purpose ====
#
# Waits until the IO thread of the current connection has stopped
# gracefully.
#
# Note: this script will fail if the IO thread stops with an error.
# If you expect an error in the IO thread, use
# include/wait_for_slave_io_error.inc instead.
#
# This script also fails if a timeout is reached (default 300
# seconds).
#
#
# ==== Usage ====
#
# [--let $rpl_channel_name= NAME]
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/wait_for_slave_io_to_stop.inc
#
# Parameters:
#   $rpl_channel_name
#     See include/wait_for_slave_param.inc
#
#   $slave_timeout
#     See include/wait_for_slave_param.inc.
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= wait_for_slave_io_to_stop.inc
--source include/begin_include_file.inc


--let $slave_param= Slave_IO_Running
--let $slave_param_value= No
--let $slave_error_param= Last_IO_Errno
--source include/wait_for_slave_param.inc
--let $slave_error_param=


--let $include_filename= wait_for_slave_io_to_stop.inc
--source include/end_include_file.inc
