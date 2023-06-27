# ==== Purpose ====
#
# Create valid MySQL Rest Service schema.
#
# ==== Usage ====
#
# --source ../include/mrs_initialize_schema.inc
#

--disable_query_log
--disable_result_log
--source ../schema/mrs_metadata_schema.sql
--enable_query_log
--enable_result_log

echo # MySQL Rest Service schema created.;
