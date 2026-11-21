# ==== Purpose ====
#
# Helper for rpl_csa_imr_reconnect_mid_trx.test: make the receiver reconnect in
# the middle of one transaction and check that the replica applies it in full.
#
# The IO thread is paused after it queued the 4th event of the transaction
# (GTID, BEGIN, TABLE_MAP, WRITE_ROWS), then the next event read is made to fail,
# so the IO thread reconnects inside the same thread. GTID auto-positioning
# makes the source resend the whole transaction, starting with a fake Rotate.
#
# ==== Usage ====
#
# --let $first_id= <first primary key of the transaction>
# --let $row_count= <number of rows in the transaction>
# --let $row_size= <bytes per row>
# --source suite/rpl_gtid/t/rpl_csa_imr_reconnect_mid_trx.inc
#
# Expects the connection to be the replica, a running channel, and table t1.

SET GLOBAL debug = '+d,pause_on_queue_event_after_write_buffer';

--source include/rpl/connection_source.inc
--let $last_id= `SELECT $first_id + $row_count - 1`
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
# Let GTID, BEGIN and TABLE_MAP through; stop after the first WRITE_ROWS.
--let $n= 1
while ($n <= 3)
{
  SET DEBUG_SYNC = 'now WAIT_FOR receiver_reached_pause_on_queue_event';
  SET DEBUG_SYNC = 'now SIGNAL receiver_continue_queuing_event';
  --inc $n
}
SET DEBUG_SYNC = 'now WAIT_FOR receiver_reached_pause_on_queue_event';

--let $qlen= query_get_value(SHOW REPLICA STATUS, In_Memory_Queue_Length, 1)
--let $assert_text= The transaction is open in the queue when the receiver reconnects
--let $assert_cond= $qlen = 1
--source include/assert.inc

# The next event read fails, so the IO thread reconnects (without exiting).
SET GLOBAL debug = '-d,pause_on_queue_event_after_write_buffer';
SET GLOBAL debug = '+d,simulate_reconnect_after_failed_event_read';
SET DEBUG_SYNC = 'now SIGNAL receiver_continue_queuing_event';

# Once the IO thread waits to reconnect, let the reads succeed again.
--let $wait_condition= SELECT COUNT(*) = 1 FROM performance_schema.threads WHERE NAME = 'thread/sql/replica_io' AND PROCESSLIST_STATE LIKE 'Waiting to reconnect%'
--source include/wait_condition.inc
SET GLOBAL debug = '-d,simulate_reconnect_after_failed_event_read';

# The resent transaction is applied in full: the partial one was truncated and
# rolled back, not sealed as complete.
--source include/rpl/connection_source.inc
--source include/rpl/sync_to_replica.inc

--let $applied= `SELECT COUNT(*) FROM t1 WHERE id BETWEEN $first_id AND $last_id`
--let $assert_text= After the reconnect the transaction is applied in full
--let $assert_cond= $applied = $row_count
--source include/assert.inc

--let $sql_errno= query_get_value(SHOW REPLICA STATUS, Last_SQL_Errno, 1)
--let $assert_text= The reconnect raises no applier error
--let $assert_cond= $sql_errno = 0
--source include/assert.inc

# A partial transaction left open would still hold its locks on t1, at least
# the IX table lock taken by its WRITE_ROWS event. Check data_locks on t1, not
# innodb_trx: innodb_trx also lists InnoDB background transactions, such as
# the GTID persister's periodic insert into mysql.gtid_executed, and it is
# served from a cache that can be up to 100 ms old. data_locks is read live.
--let $assert_text= No transaction of the partial copy holds locks on t1
--let $assert_cond= [SELECT COUNT(*) FROM performance_schema.data_locks WHERE OBJECT_SCHEMA = "test" AND OBJECT_NAME = "t1"] = 0
--source include/assert.inc

SET DEBUG_SYNC = 'RESET';
