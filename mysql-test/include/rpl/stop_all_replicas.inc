# ==== Purpose ====
#
# Stop all slaves configured by rpl/init.inc and waits for the slave
# threads to stop.
#
#
# ==== Usage ====
#
# [--let $rpl_only_running_threads= 1]
# [--let $rpl_debug= 1]
# [--let $slave_timeout= NUMBER]
# --source include/rpl/stop_all_replicas.inc
#
# Parameters:
#   $rpl_only_running_threads
#     See include/rpl/stop_replica.inc
#
#   $slave_timeout
#     Set the timeout when waiting for replica threads to stop.  See
#     include/rpl/wait_for_replica_status.inc
#
#   $rpl_debug
#     See include/rpl/init.inc


--let $include_filename= rpl/stop_all_replicas.inc
--source include/begin_include_file.inc

--let $rpl_source_file= include/rpl/stop_replica.inc
--source include/rpl/for_each_connection.inc

--let $include_filename= rpl/stop_all_replicas.inc
--source include/end_include_file.inc
