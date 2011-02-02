# ==== Purpose ====
#
# Start all slaves configured by rpl_init.inc and wait for the slave
# threads to start.
#
# Note that rpl_init.inc calls this file automatically, so you only
# need to source this file if the slaves have stopped after that.
#
#
# ==== Usage ====
#
# [--let $rpl_debug= 1]
# [--let $slave_timeout= NUMBER]
# --source include/rpl_start_slaves.inc
#
# Parameters:
#   $rpl_debug
#     See include/rpl_init.inc
#
#   $slave_timeout
#     Set the timeout when waiting for slave threads to stop and
#     start, respectively.  See include/wait_for_slave_param.inc


--let $include_filename= rpl_start_slaves.inc
--source include/begin_include_file.inc

--let $rpl_source_file= include/start_slave.inc
--source include/rpl_for_each_slave.inc

--let $include_filename= rpl_start_slaves.inc
--source include/end_include_file.inc
