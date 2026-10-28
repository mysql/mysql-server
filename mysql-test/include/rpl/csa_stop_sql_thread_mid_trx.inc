# ==== Purpose ====
#
# Interrupt the CSA applier with STOP REPLICA SQL_THREAD while a worker is in
# the MIDDLE of applying a transaction, then START REPLICA SQL_THREAD and
# verify the applier recovers with no gap: the interrupted transaction is rolled
# back (never partially applied), re-dispatched from the retained in-memory
# queue, and applied in full, and the replica ends consistent with the source.
#
# The receiver (IO thread) keeps running throughout; only the applier (SQL
# thread) is stopped and started, so the transaction stays in the in-memory
# relay log queue and is replayed from there.
#
# ==== Usage ====
#
#   --let $csa_mid_trx_label=   <text used in assertion messages>
#   --let $csa_mid_trx_id_start=<first PK value for this transaction>
#   --let $csa_mid_trx_rows=    <number of rows in the transaction>
#   --let $csa_mid_trx_bytes=   <bytes per row (>3000 to trip the pause point)>
#   --source include/rpl/csa_stop_sql_thread_mid_trx.inc
#
# Requires: a table test.t1 (id INT PRIMARY KEY, data LONGBLOB), a CSA channel
# with the in-memory relay log enabled, the applier running on entry (it is left
# running on exit), $rpl_extra_connections_per_server >= 1, and a debug +
# debug_sync build.

--let $include_filename= csa_stop_sql_thread_mid_trx.inc [$csa_mid_trx_label]
--source include/begin_include_file.inc

--let $_csa_mid_trx_id_end= `SELECT $csa_mid_trx_id_start + $csa_mid_trx_rows - 1`

# Arm the pause point: a worker parks (before releasing event memory) right
# after applying an event larger than 3000 bytes -- i.e. mid-transaction.
--source include/rpl/connection_replica.inc
--let $debug_point= pause_csa_before_releasing_event_memory
--source include/add_debug_point.inc

# One transaction whose row events are large enough to trip the pause point.
# The caller sizes it for the memory path (< spill threshold) or the spill path
# (> spill threshold).
--source include/rpl/connection_source.inc
BEGIN;
--disable_query_log
--let $_csa_i= $csa_mid_trx_id_start
while ($_csa_i <= $_csa_mid_trx_id_end)
{
  --eval INSERT INTO t1 VALUES ($_csa_i, REPEAT('x', $csa_mid_trx_bytes))
  --inc $_csa_i
}
--enable_query_log
COMMIT;

# Wait until a worker is paused in the middle of applying the transaction.
--source include/rpl/connection_replica.inc
SET DEBUG_SYNC= 'now WAIT_FOR paused_csa_before_releasing_event_memory TIMEOUT 30';

# Stop the SQL thread while the worker is paused mid-transaction. STOP blocks
# until the applier is down, so it is sent asynchronously on a second session.
--source include/rpl/connection_replica1.inc
--send STOP REPLICA SQL_THREAD

# Release the paused worker so STOP can complete. Remove the pause point first
# so the replay after START does not park again.
--source include/rpl/connection_replica.inc
--let $debug_point= pause_csa_before_releasing_event_memory
--source include/remove_debug_point.inc
SET DEBUG_SYNC= 'now SIGNAL signal_continue_csa_releasing_event_memory';

--source include/rpl/connection_replica1.inc
--reap

--source include/rpl/connection_replica.inc
SET DEBUG_SYNC= 'RESET';

# Stopping a clean in-flight session must not fail the channel.
--source include/rpl/assert_replica_no_error.inc

# No partial apply / no gap: the interrupted transaction was uncommitted, so it
# rolled back and none of its rows are visible while the applier is stopped.
--let $wait_condition= SELECT COUNT(*) = 0 FROM t1 WHERE id BETWEEN $csa_mid_trx_id_start AND $_csa_mid_trx_id_end
--source include/wait_condition.inc
--let $_csa_applied= query_get_value(SELECT COUNT(*) AS c FROM t1 WHERE id BETWEEN $csa_mid_trx_id_start AND $_csa_mid_trx_id_end, c, 1)
--let $assert_text= $csa_mid_trx_label: interrupted transaction rolled back, not partially applied
--let $assert_cond= $_csa_applied = 0
--source include/assert.inc

# The applier stopped mid-transaction must not have durably recorded the GTID:
# capture the source's executed set and assert it is NOT yet a subset of the
# replica's (the replica is missing the interrupted transaction -> a real gap
# that recovery must close).
--source include/rpl/connection_source.inc
--let $_csa_master_gtid= query_get_value(SELECT @@GLOBAL.gtid_executed AS g, g, 1)
--source include/rpl/connection_replica.inc
--disable_query_log
--eval SET @_csa_subset = GTID_SUBSET('$_csa_master_gtid', @@GLOBAL.gtid_executed)
--enable_query_log
--let $_csa_subset= query_get_value(SELECT @_csa_subset AS s, s, 1)
--let $assert_text= $csa_mid_trx_label: interrupted transaction absent from replica gtid_executed while stopped (gap present)
--let $assert_cond= $_csa_subset = 0
--source include/assert.inc

# Recover: START SQL_THREAD re-dispatches the retained transaction from the
# in-memory queue and applies it in full.
--source include/rpl/start_applier.inc

--source include/rpl/connection_source.inc
--source include/rpl/sync_to_replica.inc

--source include/rpl/connection_replica.inc
--let $_csa_applied= query_get_value(SELECT COUNT(*) AS c FROM t1 WHERE id BETWEEN $csa_mid_trx_id_start AND $_csa_mid_trx_id_end, c, 1)
--let $assert_text= $csa_mid_trx_label: transaction fully applied after START SQL_THREAD (recovered)
--let $assert_cond= $_csa_applied = $csa_mid_trx_rows
--source include/assert.inc

# No gap after recovery: the source's executed set is now a subset of the
# replica's, and the row data matches.
--source include/rpl/connection_source.inc
--let $_csa_master_gtid= query_get_value(SELECT @@GLOBAL.gtid_executed AS g, g, 1)
--source include/rpl/connection_replica.inc
--disable_query_log
--eval SET @_csa_subset = GTID_SUBSET('$_csa_master_gtid', @@GLOBAL.gtid_executed)
--enable_query_log
--let $_csa_subset= query_get_value(SELECT @_csa_subset AS s, s, 1)
--let $assert_text= $csa_mid_trx_label: replica gtid_executed covers the source after recovery (no gap)
--let $assert_cond= $_csa_subset = 1
--source include/assert.inc

# Row data matches the source. Compare id + LENGTH(data) (ordered by the small
# int key) instead of the raw LONGBLOB, so the spill-sized rows do not blow the
# sort buffer.
--let $rpl_diff_statement= SELECT id, LENGTH(data) AS len FROM t1 ORDER BY id
--source include/rpl/diff.inc

--let $include_filename= csa_stop_sql_thread_mid_trx.inc [$csa_mid_trx_label]
--source include/end_include_file.inc
