# ==== Purpose ====
#
# Create MySQL Router default user for InnoDB Cluster access.
#
# ==== Usage ====
#
# --source ../include/innodb_cluster/create_mysqlrouter_user.inc
#

--disable_query_log
--disable_result_log
--source create_mysqlrouter_user.sql
--enable_query_log
--enable_result_log

echo # MySQL Router metadata user created.;
