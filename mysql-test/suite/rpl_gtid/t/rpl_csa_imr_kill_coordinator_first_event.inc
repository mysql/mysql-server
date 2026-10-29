# Shared step of rpl_csa_imr_kill_coordinator_first_event.test.
#
# Precondition: the coordinator was just released from
# imr_pause_before_job_build and the receiver has not appended the first (GTID)
# event of the dispatched transaction, so the coordinator waits for it in the
# Job_applier constructor.
#
# KILL the coordinator and wait at most 10 s for the SQL thread to exit. If the
# KILL is not honored (the bug), print the coordinator state and release it
# with STOP REPLICA SQL_THREAD, so the test fails on the result file instead of
# hanging. Then check that the KILL left no error and the queue kept the
# transaction.
#
# Parameters: $scenario, $exited_query.

# Wait until the coordinator left the debug sync wait, i.e. it runs the
# Job_applier constructor.
--let $wait_condition= SELECT PROCESSLIST_STATE NOT LIKE 'debug sync point%' FROM performance_schema.threads WHERE NAME = 'thread/sql/replica_sql'
--source include/wait_condition.inc
# Let the first-event peek reach its poll loop.
--sleep 1

--let $sql_id= `SELECT PROCESSLIST_ID FROM performance_schema.threads WHERE NAME = 'thread/sql/replica_sql'`
--let $coord_state= `SELECT PROCESSLIST_STATE FROM performance_schema.threads WHERE NAME = 'thread/sql/replica_sql'`
--echo $scenario: coordinator state before KILL: $coord_state

--replace_result $sql_id SQL_THREAD
--eval KILL $sql_id

# A timeout must fail on the result file, not abort the test.
--let $_f16_saved_debug_info= $show_rpl_debug_info
--let $show_rpl_debug_info= 0
--let $wait_timeout= 10
--let $silent_failure= 1
--let $wait_condition= $exited_query
--source include/wait_condition.inc
--let $silent_failure= 0
--let $show_rpl_debug_info= $_f16_saved_debug_info
--echo $scenario: SQL thread exited within 10 s of KILL: $success

if (!$success)
{
  --let $coord_command= `SELECT COMMAND FROM information_schema.processlist WHERE ID = $sql_id`
  --let $coord_state= `SELECT PROCESSLIST_STATE FROM performance_schema.threads WHERE NAME = 'thread/sql/replica_sql'`
  --echo $scenario: KILL not honored: COMMAND=$coord_command STATE=$coord_state
  --echo $scenario: release the coordinator with STOP REPLICA SQL_THREAD.
  --source include/rpl/connection_replica1.inc
  --send STOP REPLICA SQL_THREAD
  --source include/rpl/connection_replica.inc
  --let $show_rpl_debug_info= 0
  --let $wait_timeout= 30
  --let $silent_failure= 1
  --let $wait_condition= $exited_query
  --source include/wait_condition.inc
  --let $silent_failure= 0
  --let $show_rpl_debug_info= $_f16_saved_debug_info
  --echo $scenario: SQL thread exited within 30 s of STOP REPLICA SQL_THREAD: $success
  --source include/rpl/connection_replica1.inc
  --reap
  --source include/rpl/connection_replica.inc
}

--let $assert_text= $scenario: the KILL raises no applier error
--let $assert_cond= [SHOW REPLICA STATUS, Last_SQL_Errno, 1] = 0
--source include/assert.inc
--let $assert_text= $scenario: the IO thread keeps running
--let $assert_cond= "[SHOW REPLICA STATUS, Replica_IO_Running, 1]" = "Yes"
--source include/assert.inc
--let $assert_text= $scenario: the queue keeps the killed transaction
--let $assert_cond= [SHOW REPLICA STATUS, In_Memory_Queue_Length, 1] >= 1
--source include/assert.inc
