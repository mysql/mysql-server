# ==== Purpose ====
#
# Waits until both the IO and SQL threads of the current connection
# have stopped gracefully.
#
# Note: this script will fail if one of the threads stops with an
# error.  If you expect an error in one of the threads, use
# include/wait_for_slave_io_error.inc or
# include/wait_for_slave_sql_error.inc instead.
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
# --source include/wait_for_slave_to_stop.inc
#
# Parameters:
#   $rpl_channel_name
#     See include/wait_for_slave_param.inc
#
#   $slave_timeout
#     See include/wait_for_slave_param.inc
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= wait_for_slave_to_stop.inc
if ($rpl_channel_name)
{
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc


--source include/wait_for_slave_sql_to_stop.inc
--source include/wait_for_slave_io_to_stop.inc


--let $include_filename= wait_for_slave_to_stop.inc
--source include/end_include_file.inc
