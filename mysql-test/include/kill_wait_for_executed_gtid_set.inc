# ==== Purpose ====
#
# Kill an ongoing WAIT_FOR_EXECUTED_GTID_SET query on a specified connection.
#
# ==== Usage ====
#
# --let $thread_id= THREAD_ID
# --let $rpl_connection_name= CONNECTION
# --source include/kill_wait_for_executed_gtid_set.inc
#
# Parameters:
#   $thread_id
#     Thread_id of the ongoing WAIT_FOR_EXECUTED_GTID_SET
#
#   $rpl_connection_name
#     The connection where WAIT_FOR_EXECUTED_GTID_SET is executing.
#     The script will execute 'reap' on this connection.

--let $include_filename= kill_wait_for_executed_gtid_set.inc
--source include/begin_include_file.inc

if ($rpl_connection_name == '') {
  --die !!!ERROR IN TEST: Set $rpl_connection_name before you source kill_wait_for_executed_gtid_set.inc
}

# Kill
--disable_query_log
--disable_result_log
eval KILL QUERY $thread_id;

# Wait for query to stop executing query.
--let $wait_condition= SELECT COUNT(*) = 0 FROM performance_schema.threads WHERE PROCESSLIST_ID = $thread_id AND PROCESSLIST_INFO LIKE '%WAIT_FOR_EXECUTED_GTID_SET%'
--source include/wait_condition.inc

# Reap killed query
--source include/rpl_connection.inc
--error ER_QUERY_INTERRUPTED
--reap


--let $include_filename= kill_wait_for_executed_gtid_set.inc
--source include/end_include_file.inc
