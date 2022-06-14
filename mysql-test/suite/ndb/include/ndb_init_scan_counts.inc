--disable_query_log
--disable_result_log
select VARIABLE_VALUE
  from performance_schema.session_status
  where variable_name = 'NDB_SCAN_COUNT' into @ndb_init_scan_count;
select VARIABLE_VALUE
  from performance_schema.session_status
  where variable_name = 'NDB_PRUNED_SCAN_COUNT' into @ndb_init_pruned_scan_count;
select @ndb_init_scan_count, @ndb_init_pruned_scan_count;
--enable_query_log
--enable_result_log
