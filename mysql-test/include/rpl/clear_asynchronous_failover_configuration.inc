# ==== Purpose ====
#
# Clear replication asynchronous failover configuration tables
# on the current connection.
#
# ==== Usage ====
#
# --source include/rpl/clear_asynchronous_failover_configuration.inc
#

--let $include_filename= rpl/clear_asynchronous_failover_configuration.inc
--source include/begin_include_file.inc

--disable_query_log ONCE
--disable_result_log ONCE
SELECT asynchronous_connection_failover_reset();

--source include/end_include_file.inc
