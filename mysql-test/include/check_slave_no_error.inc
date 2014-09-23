# ==== Purpose ====
#
# Assert that Slave_SQL_Errno = Slave_IO_Errno = 0 in the output from
# SHOW SLAVE STATUS.
#
#
# ==== Usage ====
#
# [--let $rpl_channel_name = NAME]
# [--let $rpl_debug= 1]
# --source include/check_slave_no_error.inc
#
# Parameters:
#   $rpl_channel_name
#     See include/check_slave_param.inc
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= check_slave_no_error.inc
--source include/begin_include_file.inc


--let $slave_param= Last_SQL_Errno
--let $slave_param_value= 0
--source include/check_slave_param.inc

--let $slave_param= Last_IO_Errno
--let $slave_param_value= 0
--source include/check_slave_param.inc


--let $include_filename= check_slave_no_error.inc
--source include/end_include_file.inc
