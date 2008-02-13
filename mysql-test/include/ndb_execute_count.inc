--disable_query_log
select @ndb_execute_count:=VARIABLE_VALUE-@ndb_init_execute_count from information_schema.session_status where variable_name like 'NDB_EXECUTE_COUNT';
--enable_query_log
