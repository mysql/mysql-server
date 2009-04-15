--disable_query_log
select @ndb_scan_count:=VARIABLE_VALUE-@ndb_init_scan_count AS ndb_scan_count from information_schema.session_status where variable_name like 'NDB_SCAN_COUNT';
select @ndb_pruned_scan_count:=VARIABLE_VALUE-@ndb_init_pruned_scan_count AS ndb_pruned_scan_count from information_schema.session_status where variable_name like 'NDB_PRUNED_SCAN_COUNT';
--enable_query_log
