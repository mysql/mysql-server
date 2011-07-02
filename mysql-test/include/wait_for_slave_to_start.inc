# ==== Purpose ====
#
# Waits until both the IO and SQL threads of the current connection
# have started, or until a timeout is reached.
#
#
# ==== Usage ====
#
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/wait_for_slave_to_start.inc
#
# Parameters:
#   $slave_timeout
#     See include/wait_for_slave_param.inc
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= wait_for_slave_to_start.inc
--source include/begin_include_file.inc


--source include/wait_for_slave_io_to_start.inc
--source include/wait_for_slave_sql_to_start.inc


--let $include_filename= wait_for_slave_to_start.inc
--source include/end_include_file.inc
