# ==== Purpose ====
#
# Start all slaves configured by rpl/init.inc and wait for the slave
# threads to start.
#
# Note that rpl/init.inc calls this file automatically, so you only
# need to source this file if the slaves have stopped after that.
#
#
# ==== Usage ====
#
# [--let $rpl_debug= 1]
# [--let $slave_timeout= NUMBER]
# --source include/rpl/start_all_replicas.inc
#
# Parameters:
#   $rpl_debug
#     See include/rpl/init.inc
#
#   $slave_timeout
#     Set the timeout when waiting for replica threads to stop and
#     start, respectively.  See include/rpl/wait_for_replica_status.inc


--let $include_filename= rpl/start_all_replicas.inc
--source include/begin_include_file.inc

--let $rpl_source_file= include/rpl/start_replica.inc
--source include/rpl/for_each_connection.inc

--let $include_filename= rpl/start_all_replicas.inc
--source include/end_include_file.inc
