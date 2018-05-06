--disable_query_log
--disable_result_log
SELECT @ndb_init_schema_locks_count:=VARIABLE_VALUE
  FROM performance_schema.session_status
  WHERE variable_name = 'NDB_SCHEMA_LOCKS_COUNT';
--enable_query_log
--enable_result_log
