--disable_query_log
select VARIABLE_VALUE-@ndb_init_scan_count AS ndb_scan_count
  from performance_schema.session_status
  where variable_name = 'NDB_SCAN_COUNT';
select VARIABLE_VALUE-@ndb_init_pruned_scan_count AS ndb_pruned_scan_count
  from performance_schema.session_status
  where variable_name = 'NDB_PRUNED_SCAN_COUNT';
--enable_query_log
