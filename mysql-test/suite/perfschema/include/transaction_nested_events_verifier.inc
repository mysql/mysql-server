# Tests for the performance schema

# =====================================================
# HELPER include/transaction_nested_events_verifier.inc
# =====================================================

--connection default
--disable_query_log

# Wait a short time before jumping into the queries
let $wait_timeout= 10;
let $wait_condition=
  SELECT COUNT(*) > 0 FROM performance_schema.events_statements_history_long
  WHERE thread_id = @con1_thread_id;
--source include/wait_condition.inc

let $wait_timeout= 10;
let $wait_condition=
  SELECT COUNT(*) > 0 FROM performance_schema.events_transactions_history_long
  WHERE thread_id = @con1_thread_id;
--source include/wait_condition.inc

--echo #========================================================================
--echo # Verify
--echo #========================================================================

--echo EVENTS_TRANSACTIONS_CURRENT
--echo

--replace_column 1 thread_id 2 event_id 8 nesting_event_id

#SELECT THREAD_ID, EVENT_ID, EVENT_NAME, STATE, ACCESS_MODE, ISOLATION_LEVEL, AUTOCOMMIT, NESTING_EVENT_ID, NESTING_EVENT_TYPE
SELECT THREAD_ID, EVENT_ID,
  RPAD(EVENT_NAME, 11, ' ') 'EVENT_NAME ',
  RPAD(STATE, 11, ' ') 'STATE      ',
  RPAD(ACCESS_MODE, 11, ' ') ACCESS_MODE,
  RPAD(ISOLATION_LEVEL, 16, ' ') 'ISOLATION_LEVEL ',
  RPAD(AUTOCOMMIT, 4, ' ') AUTO,
  NESTING_EVENT_ID,
  RPAD(IFNULL(NESTING_EVENT_TYPE, 'NULL'), 18, ' ') NESTING_EVENT_TYPE
FROM performance_schema.events_transactions_current
WHERE (THREAD_ID = @con1_thread_id OR (@all_threads = 1))
ORDER BY event_id;

--disable_parsing
--echo
--echo EVENTS_TRANSACTIONS_HISTORY
--echo

--replace_column 1 thread_id 2 event_id 8 nesting_event_id

#SELECT THREAD_ID, EVENT_ID, EVENT_NAME, STATE, ACCESS_MODE, ISOLATION_LEVEL, AUTOCOMMIT, NESTING_EVENT_ID, NESTING_EVENT_TYPE
SELECT THREAD_ID, EVENT_ID,
  RPAD(EVENT_NAME, 11, ' ') 'EVENT_NAME ',
  RPAD(STATE, 11, ' ') 'STATE      ',
  RPAD(ACCESS_MODE, 11, ' ') ACCESS_MODE,
  RPAD(ISOLATION_LEVEL, 16, ' ') 'ISOLATION_LEVEL ',
  RPAD(AUTOCOMMIT, 4, ' ') AUTO,
  NESTING_EVENT_ID,
  RPAD(IFNULL(NESTING_EVENT_TYPE, 'NULL'), 18, ' ') NESTING_EVENT_TYPE
FROM performance_schema.events_transactions_history
WHERE (THREAD_ID = @con1_thread_id OR (@all_threads = 1))
ORDER BY event_id;
--enable_parsing

--echo
--echo EVENTS_TRANSACTIONS_HISTORY_LONG
--echo

--replace_column 1 thread_id 2 event_id 8 nesting_event_id

# SELECT THREAD_ID, EVENT_ID, EVENT_NAME, STATE, ACCESS_MODE, ISOLATION_LEVEL, AUTOCOMMIT, NESTING_EVENT_ID, NESTING_EVENT_TYPE
SELECT THREAD_ID, EVENT_ID,
  RPAD(EVENT_NAME, 11, ' ') 'EVENT_NAME ',
  RPAD(STATE, 11, ' ') 'STATE      ',
  RPAD(ACCESS_MODE, 11, ' ') ACCESS_MODE,
  RPAD(ISOLATION_LEVEL, 16, ' ') 'ISOLATION_LEVEL ',
  RPAD(AUTOCOMMIT, 4, ' ') AUTO,
  NESTING_EVENT_ID,
  RPAD(IFNULL(NESTING_EVENT_TYPE, 'NULL'), 18, ' ') NESTING_EVENT_TYPE
FROM performance_schema.events_transactions_history_long
WHERE (THREAD_ID = @con1_thread_id OR (@all_threads = 1))
ORDER BY event_id;

--disable_parsing
--echo
--echo EVENTS_STATEMENTS_HISTORY_LONG
--echo

--replace_column 1 thread_id 2 event_id 5 nesting_event_id

SELECT THREAD_ID, EVENT_ID,
  RPAD(EVENT_NAME, 30, ' ') 'EVENT_NAME                    ',
  RPAD(IFNULL(object_name, 'NULL'), 12, ' ') 'OBJECT_NAME ',
  NESTING_EVENT_ID,
  RPAD(IFNULL(NESTING_EVENT_TYPE, 'NULL'), 18, ' ') NESTING_EVENT_TYPE,
  LPAD(NESTING_EVENT_LEVEL, 5, ' ') LEVEL,
  SQL_TEXT
FROM performance_schema.events_statements_history_long
WHERE (THREAD_ID = @con1_thread_id OR (@all_threads = 1))
ORDER BY event_id;

--echo
--echo ## Statements that are parents of transactions
--echo
--echo EVENTS_STATEMENTS_HISTORY_LONG :: EVENTS_TRANSACTIONS_HISTORY
--echo

--replace_column  2 event_id 5 event_id 9 nesting_event_id

SELECT '<parent>' AS 'PARENT  ',
  s.EVENT_ID,
  RPAD(s.EVENT_NAME, 25, ' ') 'EVENT_NAME               ',
  '<child>' AS 'CHILD  ',
  t.EVENT_ID,
  RPAD(t.EVENT_NAME, 25, ' ') 'EVENT_NAME               ',
  RPAD(t.STATE, 11, ' ') 'STATE      ',
  RPAD(t.AUTOCOMMIT, 4, ' ') AUTO,
  t.NESTING_EVENT_ID,
  RPAD(IFNULL(t.NESTING_EVENT_TYPE, 'NULL'), 18, ' ') NESTING_EVENT_TYPE,
  s.SQL_TEXT
FROM performance_schema.events_statements_history_long s
JOIN performance_schema.events_transactions_history t
WHERE t.nesting_event_id = s.event_id
  AND t.thread_id = s.thread_id
  AND (t.thread_id = @con1_thread_id OR (@all_threads = 1))
ORDER BY s.event_id, t.event_id;

--echo
--echo ## Transactions that are parents of statements
--echo
--echo EVENTS_TRANSACTIONS_HISTORY :: EVENTS_STATEMENTS_HISTORY_LONG
--echo

--replace_column  2 event_id 7 event_id 9 nesting_event_id

SELECT '<parent>' AS 'PARENT  ',
  t.EVENT_ID,
  RPAD(t.EVENT_NAME, 25, ' ') 'EVENT_NAME               ',
  RPAD(t.STATE, 11, ' ') 'STATE      ',
  RPAD(t.AUTOCOMMIT, 4, ' ') AUTO,
  '<child>' AS 'CHILD  ',
  s.EVENT_ID,
  RPAD(s.EVENT_NAME, 25, ' ') 'EVENT_NAME               ',
  s.NESTING_EVENT_ID,
  RPAD(IFNULL(s.NESTING_EVENT_TYPE, 'NULL'), 18, ' ') NESTING_EVENT_TYPE,
  LPAD(s.NESTING_EVENT_LEVEL, 5, ' ') LEVEL,
  s.SQL_TEXT
FROM performance_schema.events_transactions_history t
JOIN performance_schema.events_statements_history_long s
WHERE s.nesting_event_id = t.event_id
  AND s.thread_id = t.thread_id
  AND (s.thread_id = @con1_thread_id OR (@all_threads = 1))
ORDER BY t.event_id, s.event_id;

--enable_parsing

--echo
--echo ## Combined statement and transaction event history ordered by event id
--echo
--echo EVENTS_STATEMENTS_HISTORY_LONG + EVENTS_TRANSACTIONS_HISTORY_LONG
--echo

--replace_column 1 event_id 3 nesting_event_id

SELECT EVENT_ID,
  RPAD(EVENT_NAME, 25, ' ') 'EVENT_NAME               ',
  NESTING_EVENT_ID,
  RPAD(IFNULL(NESTING_EVENT_TYPE, 'NULL'), 18, ' ') NESTING_EVENT_TYPE,
  '<transaction started>' AS SQL_TXT
FROM performance_schema.events_transactions_history_long t
WHERE t.thread_id = @con1_thread_id
UNION
SELECT EVENT_ID,
  RPAD(EVENT_NAME, 25, ' ') 'EVENT_NAME               ',
  NESTING_EVENT_ID,
  RPAD(IFNULL(NESTING_EVENT_TYPE, 'NULL'), 18, ' ') NESTING_EVENT_TYPE,
  SQL_TEXT
FROM performance_schema.events_statements_history_long s
WHERE (s.thread_id = @con1_thread_id OR (@all_threads = 1))
ORDER BY event_id;

--echo
--echo ## Clear statement and transaction history
--echo CALL test.clear_history();
CALL test.clear_history();
--enable_query_log
--echo ## Reset db.t1
DELETE FROM db.t1;
--echo
