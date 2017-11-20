--disable_result_log
--disable_query_log
--error 0, 1109
# Note this .inc file doesn't check if plugin is enabled or active. This
# is intentional. If NDB plugin is loaded, the .inc will skip the test.
select @have_ndb:= count(*) from information_schema.engines where engine='ndbcluster';
if (`select @have_ndb`) {
  --skip "Cannot run on ndb builds"
}
--enable_query_log
--enable_result_log
