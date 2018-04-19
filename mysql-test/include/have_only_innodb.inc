--disable_result_log
--disable_query_log
--error 0, 1109
# Note this .inc file doesn't check if plugin is enabled or active. This
# is intentional. If other plugin wihch creates system table is loaded, the .inc will skip the test.

# Check that ndbcluster(which would create mysql.ndb_binlog_index table) is unknown
select @have_ndb:= count(*) from information_schema.engines where engine='ndbcluster';
if (`select @have_ndb`) {
  --skip Test fails when InnoDB is not the only engine
}
# Add below any checks which may interfer with InnoDB
--enable_query_log
--enable_result_log
