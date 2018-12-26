# ==== Purpose ====
#
# Wait until WAIT_FOR_EXECUTED_GTID_SET goes to the wait state.
#
# ==== Usage ====
#
# --let $thread_id= THREAD_ID
# --source include/wait_for_wait_for_executed_gtid_set.inc

--let $include_filename= wait_for_wait_for_executed_gtid_set.inc
--source include/begin_include_file.inc


--let $wait_condition= SELECT COUNT(*) = 1 FROM performance_schema.threads WHERE PROCESSLIST_ID = $thread_id AND PROCESSLIST_STATE = 'Waiting for GTID to be committed'
--source include/wait_condition.inc


--let $include_filename= wait_for_wait_for_executed_gtid_set.inc
--source include/end_include_file.inc
