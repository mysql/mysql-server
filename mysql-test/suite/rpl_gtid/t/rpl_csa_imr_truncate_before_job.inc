# ==== Purpose ====
#
# Helper for rpl_csa_imr_truncate_before_job.test: truncate one transaction
# after the coordinator dispatched it and before it built its job, then check
# that the channel keeps applying.
#
# The receiver is paused after the GTID event, and the coordinator after
# sweep_and_dispatch() handed out the transaction and before it builds the
# Job_applier (debug point imr_pause_before_job_build). The next event read
# then fails, so the receiver reconnects; the fake Rotate of the new connection
# truncates the open transaction and releases it. Only then is the
# coordinator let go.
#
# ==== Usage ====
#
# --let $f6_case= A | B
#   A: the replica has not executed the GTID; the source resends the
#      transaction after the reconnect.
#   B: the replica already executed the GTID; the source sends nothing.
# --let $first_id= <first primary key of the transaction>
# --let $row_count= <number of rows in the transaction>
# --let $row_size= <bytes per row>
# --source suite/rpl_gtid/t/rpl_csa_imr_truncate_before_job.inc
#
# Expects the connection to be the replica, a running channel with
# SOURCE_CONNECT_RETRY = 1, table t1, and $source_uuid. The checks after the
# release use wait_condition.inc (30 s) and a 30 s rpl_stop_replica_timeout,
# so a stuck worker fails the test instead of hanging it.
--let $last_id= `SELECT $first_id + $row_count - 1`
if ($f6_case == B)
{
  --echo # The replica executes the GTID the source assigns next.
  --source include/rpl/connection_source.inc
  --let $gno= `SELECT CAST(SUBSTRING_INDEX(SUBSTRING_INDEX(@@GLOBAL.gtid_executed, ':', -1), '-', -1) AS UNSIGNED) + 1`
  --source include/rpl/connection_replica.inc
  --replace_regex /'[^']*'/'SOURCE_UUID:GNO'/
  --eval SET GTID_NEXT = '$source_uuid:$gno'
  BEGIN;
  COMMIT;
  SET GTID_NEXT = AUTOMATIC;
}

--echo # Pause the receiver after the GTID event, and the coordinator before it
--echo # builds the job.
SET GLOBAL debug = '+d,pause_on_queue_event_after_write_buffer';
SET GLOBAL debug = '+d,imr_pause_before_job_build';
--source include/rpl/connection_source.inc
BEGIN;
--disable_query_log
--let $i= $first_id
while ($i <= $last_id)
{
  --eval INSERT INTO t1 VALUES ($i, REPEAT('x', $row_size))
  --inc $i
}
--enable_query_log
COMMIT;
--source include/rpl/connection_replica.inc
SET DEBUG_SYNC = 'now WAIT_FOR receiver_reached_pause_on_queue_event';
SET DEBUG_SYNC = 'now WAIT_FOR coordinator_reached_before_job_build';
SET GLOBAL debug = '-d,imr_pause_before_job_build';
# The previous transaction's committed envelope is swept only by the next
# dispatch, so it may still be counted here.
--let $qlen= query_get_value(SHOW REPLICA STATUS, In_Memory_Queue_Length, 1)

--echo # The receiver reconnects. Pause it again at the first event of the new
--echo # connection, the fake Rotate, which truncates the open transaction.
SET GLOBAL debug = '-d,pause_on_queue_event_after_write_buffer';
SET GLOBAL debug = '+d,simulate_reconnect_after_failed_event_read';
SET DEBUG_SYNC = 'now SIGNAL receiver_continue_queuing_event';
--let $wait_condition= SELECT COUNT(*) = 1 FROM performance_schema.threads WHERE NAME = 'thread/sql/replica_io' AND PROCESSLIST_STATE LIKE 'Waiting to reconnect%'
--source include/wait_condition.inc
SET GLOBAL debug = '-d,simulate_reconnect_after_failed_event_read';
SET GLOBAL debug = '+d,pause_on_queue_event_after_write_buffer';
SET DEBUG_SYNC = 'now WAIT_FOR receiver_reached_pause_on_queue_event';
SET GLOBAL debug = '-d,pause_on_queue_event_after_write_buffer';
SET DEBUG_SYNC = 'now SIGNAL receiver_continue_queuing_event';
if ($f6_case == A)
{
  # The resent copy is a new envelope.
  --let $slave_param= In_Memory_Queue_Length
  --let $slave_param_value= `SELECT $qlen + 1`
  --source include/rpl/wait_for_replica_status.inc
}

--echo # Let the coordinator build the job of the truncated transaction.
SET DEBUG_SYNC = 'now SIGNAL coordinator_continue_job_build';

if ($f6_case == A)
{
  --echo # The resent copy is applied in full.
  --let $wait_condition= SELECT COUNT(*) = $row_count FROM t1 WHERE id BETWEEN $first_id AND $last_id
  --source include/wait_condition.inc
  --source include/rpl/connection_source.inc
  --source include/rpl/sync_to_replica.inc
}

if ($f6_case == B)
{
  --echo # Following transactions replicate; the skipped rows are not applied.
  --source include/rpl/connection_source.inc
  --let $after_id= `SELECT $last_id + 1`
  --eval INSERT INTO t1 VALUES ($after_id, 'after')
  --inc $after_id
  --eval INSERT INTO t1 VALUES ($after_id, 'after')
  --source include/rpl/connection_replica.inc
  --let $wait_condition= SELECT COUNT(*) = 2 FROM t1 WHERE id > $last_id AND id <= $after_id
  --source include/wait_condition.inc
  --let $assert_text= The truncated transaction's rows are not applied
  --let $assert_cond= [SELECT COUNT(*) FROM t1 WHERE id BETWEEN $first_id AND $last_id] = 0
  --source include/assert.inc
  # Make the source match the replica again.
  --source include/rpl/connection_source.inc
  SET sql_log_bin = 0;
  --eval DELETE FROM t1 WHERE id BETWEEN $first_id AND $last_id
  SET sql_log_bin = 1;
  --source include/rpl/connection_replica.inc
}

--echo # No worker is left applying, and no GTID, lock or error is left.
--let $wait_condition= SELECT COUNT(*) = 0 FROM performance_schema.threads WHERE NAME = 'thread/sql/replica_worker' AND PROCESSLIST_STATE = 'Job: Applying.'
--source include/wait_condition.inc
--let $assert_text= No GTID is owned
--let $assert_cond= "[SELECT @@GLOBAL.gtid_owned]" = ""
--source include/assert.inc
--let $assert_text= No transaction holds locks on t1
--let $assert_cond= [SELECT COUNT(*) FROM performance_schema.data_locks WHERE OBJECT_SCHEMA = "test" AND OBJECT_NAME = "t1"] = 0
--source include/assert.inc
--let $wait_condition= SELECT COUNT(*) = 0 FROM performance_schema.metadata_locks WHERE OBJECT_SCHEMA = 'test' AND OBJECT_NAME = 't1'
--source include/wait_condition.inc
--let $sql_errno= query_get_value(SHOW REPLICA STATUS, Last_SQL_Errno, 1)
--let $assert_text= The truncation raises no applier error
--let $assert_cond= $sql_errno = 0
--source include/assert.inc
--let $slave_param= In_Memory_Queue_Length
--let $slave_param_value= 0
--source include/rpl/wait_for_replica_status.inc

--echo # STOP REPLICA SQL_THREAD returns.
SET @saved_stop_timeout= @@GLOBAL.rpl_stop_replica_timeout;
SET GLOBAL rpl_stop_replica_timeout= 30;
STOP REPLICA SQL_THREAD;
SET GLOBAL rpl_stop_replica_timeout= @saved_stop_timeout;
--let $assert_text= The SQL thread stopped
--let $assert_cond= "[SHOW REPLICA STATUS, Replica_SQL_Running, 1]" = "No"
--source include/assert.inc
START REPLICA SQL_THREAD;
--source include/rpl/wait_for_applier_to_start.inc
SET DEBUG_SYNC = 'RESET';
