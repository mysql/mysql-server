--disable_query_log
eval select VARIABLE_VALUE-@ndb_init_execute_count
     from performance_schema.session_status
     where variable_name='NDB_EXECUTE_COUNT' into @ndb_execute_count;
eval update $temp_table set $temp_col=@ndb_execute_count where pk=$temp_pk;
--enable_query_log
