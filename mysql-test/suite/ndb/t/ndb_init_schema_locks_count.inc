# Ignore information_schema.session_status deprecation warning
--disable_warnings

--disable_query_log
--disable_result_log
SELECT @ndb_init_schema_locks_count:=VARIABLE_VALUE
  FROM information_schema.session_status
  WHERE variable_name = 'NDB_SCHEMA_LOCKS_COUNT';
--enable_query_log
--enable_result_log

--enable_warnings
