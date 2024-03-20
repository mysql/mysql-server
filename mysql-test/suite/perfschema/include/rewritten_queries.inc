
#
# Checks to perform after each query.
#
# Parameters:
# - $con1_thread_id
# - $con1_connection_id
#

--disable_query_log
eval SELECT "thread" as mark, PROCESSLIST_INFO
  FROM performance_schema.threads
  WHERE THREAD_ID = $con1_thread_id;

eval SELECT "processlist" as mark, INFO
  FROM performance_schema.processlist
  WHERE ID = $con1_connection_id;

eval SELECT "I_S.PROCESSLIST" as mark, INFO
  FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE ID = $con1_connection_id;

eval SELECT "current" as mark, EVENT_NAME, SQL_TEXT, DIGEST_TEXT
  FROM performance_schema.events_statements_current
  WHERE THREAD_ID = $con1_thread_id;

eval SELECT "history" as mark, EVENT_NAME, SQL_TEXT, DIGEST_TEXT
  FROM performance_schema.events_statements_history
  WHERE THREAD_ID = $con1_thread_id
  ORDER BY EVENT_ID;

eval SELECT "history_long" as mark, EVENT_NAME, SQL_TEXT, DIGEST_TEXT
  FROM performance_schema.events_statements_history_long
  WHERE THREAD_ID = $con1_thread_id
  ORDER BY EVENT_ID;
--enable_query_log
