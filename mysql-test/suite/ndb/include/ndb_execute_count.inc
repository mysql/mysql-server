--disable_query_log
select @ndb_execute_count:=VARIABLE_VALUE-@ndb_init_execute_count
  from performance_schema.session_status
  where variable_name = 'NDB_EXECUTE_COUNT';
--enable_query_log
