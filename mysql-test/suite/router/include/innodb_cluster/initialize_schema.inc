# ==== Purpose ====
#
# Create valid MySQL InnoDB cluster schema.
#
# ==== Usage ====
#
# --source ../include/innodb_cluster/initialize_schema.inc
#

--disable_query_log
--disable_result_log
--source ../schema/innodb_cluster_metadata_schema.sql
--enable_query_log
--enable_result_log

echo # MySQL InnoDB Cluster schema created.;
