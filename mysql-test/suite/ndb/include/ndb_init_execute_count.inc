--disable_query_log
--disable_result_log
select @ndb_init_execute_count:=VARIABLE_VALUE
  from performance_schema.session_status
  where variable_name = 'NDB_EXECUTE_COUNT';
--enable_query_log
--enable_result_log
