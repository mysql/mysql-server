# Tests for the performance schema

# =====================================================
# HELPER include/transaction_nested_events_verifier.inc
# =====================================================

--connection default
--disable_query_log

# Poll till the activity of the actor connection con1 caused by the last
# statement issued has finished.
let $wait_timeout= 10;
let $wait_condition=
  SELECT COUNT(*) > 0 FROM performance_schema.threads
  WHERE THREAD_ID = @con1_thread_id
    AND (PROCESSLIST_STATE = 'Sleep' OR PROCESSLIST_STATE IS NULL);
--source include/wait_condition.inc
if(!$success)
{
  --echo The activity of connection con1 did not finish.
  SELECT thread_id , PROCESSLIST_id, PROCESSLIST_STATE, PROCESSLIST_INFO
  FROM performance_schema.threads
  WHERE thread_id = @con1_thread_id;
  --echo Abort.
  exit;
}

--echo #========================================================================
--echo # Verify
--echo #========================================================================

SELECT event_id from performance_schema.events_transactions_history_long
  where (THREAD_ID = @con1_thread_id)
  order by event_id limit 1
  into @base_tx_event_id;

SELECT event_id from performance_schema.events_statements_history_long
  where (THREAD_ID = @con1_thread_id)
  order by event_id limit 1
  into @base_stmt_event_id;

select if(@base_tx_event_id < @base_stmt_event_id,
          @base_tx_event_id - 1,
          @base_stmt_event_id - 1)
  into @base_event_id;

# Debug helpers
# set @base_event_id = 0;
# select @base_tx_event_id, @base_stmt_event_id, @base_event_id;

--echo EVENTS_TRANSACTIONS_CURRENT
--echo

--replace_column 1 thread_id

SELECT THREAD_ID,
  LPAD(EVENT_ID - @base_event_id, 11, ' ') as R_EVENT_ID,
  LPAD(END_EVENT_ID - @base_event_id, 11, ' ') as R_END_EVENT_ID,
  RPAD(EVENT_NAME, 11, ' ') 'EVENT_NAME ',
  RPAD(STATE, 11, ' ') 'STATE      ',
  RPAD(ACCESS_MODE, 11, ' ') ACCESS_MODE,
  RPAD(ISOLATION_LEVEL, 16, ' ') 'ISOLATION_LEVEL ',
  RPAD(AUTOCOMMIT, 4, ' ') AUTO,
  LPAD(NESTING_EVENT_ID - @base_event_id, 19, ' ') as R_NESTING_EVENT_ID,
  RPAD(IFNULL(NESTING_EVENT_TYPE, 'NULL'), 18, ' ') NESTING_EVENT_TYPE
FROM performance_schema.events_transactions_current
WHERE ((THREAD_ID = @con1_thread_id) OR (@all_threads = 1))
ORDER BY thread_id, event_id;

--echo
--echo EVENTS_TRANSACTIONS_HISTORY_LONG
--echo

--replace_column 1 thread_id

SELECT THREAD_ID,
  LPAD(EVENT_ID - @base_event_id, 11, ' ') as R_EVENT_ID,
  LPAD(END_EVENT_ID - @base_event_id, 11, ' ') as R_END_EVENT_ID,
  RPAD(EVENT_NAME, 11, ' ') 'EVENT_NAME ',
  RPAD(STATE, 11, ' ') 'STATE      ',
  RPAD(ACCESS_MODE, 11, ' ') ACCESS_MODE,
  RPAD(ISOLATION_LEVEL, 16, ' ') 'ISOLATION_LEVEL ',
  RPAD(AUTOCOMMIT, 4, ' ') AUTO,
  LPAD(NESTING_EVENT_ID - @base_event_id, 19, ' ') as R_NESTING_EVENT_ID,
  RPAD(IFNULL(NESTING_EVENT_TYPE, 'NULL'), 18, ' ') NESTING_EVENT_TYPE
FROM performance_schema.events_transactions_history_long
WHERE ((THREAD_ID = @con1_thread_id) OR (@all_threads = 1))
ORDER BY thread_id, event_id;

--echo
--echo EVENTS_STATEMENTS_HISTORY_LONG
--echo

--replace_column 1 thread_id

SELECT THREAD_ID,
  LPAD(EVENT_ID - @base_event_id, 11, ' ') as R_EVENT_ID,
  LPAD(END_EVENT_ID - @base_event_id, 11, ' ') as R_END_EVENT_ID,
  RPAD(EVENT_NAME, 30, ' ') 'EVENT_NAME                    ',
  RPAD(IFNULL(object_name, 'NULL'), 12, ' ') 'OBJECT_NAME ',
  LPAD(IFNULL(NESTING_EVENT_ID - @base_event_id, 'NULL'), 19, ' ') as R_NESTING_EVENT_ID,
  RPAD(IFNULL(NESTING_EVENT_TYPE, 'NULL'), 18, ' ') NESTING_EVENT_TYPE,
  LPAD(NESTING_EVENT_LEVEL, 5, ' ') LEVEL,
  SQL_TEXT
FROM performance_schema.events_statements_history_long
WHERE ((THREAD_ID = @con1_thread_id) OR (@all_threads = 1))
ORDER BY thread_id, event_id;

--echo
--echo ## Combined statement and transaction event history ordered by event id
--echo
--echo EVENTS_STATEMENTS_HISTORY_LONG + EVENTS_TRANSACTIONS_HISTORY_LONG
--echo

--replace_column 1 thread_id

SELECT THREAD_ID,
  LPAD(EVENT_ID - @base_event_id, 11, ' ') as R_EVENT_ID,
  LPAD(END_EVENT_ID - @base_event_id, 11, ' ') as R_END_EVENT_ID,
  RPAD(EVENT_NAME, 25, ' ') 'EVENT_NAME               ',
  LPAD(NESTING_EVENT_ID - @base_event_id, 19, ' ') as R_NESTING_EVENT_ID,
  RPAD(IFNULL(NESTING_EVENT_TYPE, 'NULL'), 18, ' ') NESTING_EVENT_TYPE,
  '<transaction started>' AS SQL_TXT
FROM performance_schema.events_transactions_history_long t
WHERE (t.thread_id = @con1_thread_id)
UNION
SELECT THREAD_ID,
  LPAD(EVENT_ID - @base_event_id, 11, ' ') as R_EVENT_ID,
  LPAD(END_EVENT_ID - @base_event_id, 11, ' ') as R_END_EVENT_ID,
  RPAD(EVENT_NAME, 25, ' ') 'EVENT_NAME               ',
  LPAD(IFNULL(NESTING_EVENT_ID - @base_event_id, 'NULL'), 19, ' ') as R_NESTING_EVENT_ID,
  RPAD(IFNULL(NESTING_EVENT_TYPE, 'NULL'), 18, ' ') NESTING_EVENT_TYPE,
  SQL_TEXT
FROM performance_schema.events_statements_history_long s
WHERE ((s.thread_id = @con1_thread_id) OR (@all_threads = 1))
ORDER BY thread_id, r_event_id;

--echo
--echo ## Clear statement and transaction history
--echo CALL test.clear_history();
CALL test.clear_history();
--enable_query_log
--echo ## Reset db.t1
DELETE FROM db.t1;
--echo
