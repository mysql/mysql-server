# ==== Purpose ====
#
# Loop through a number of existing server connection to print out
# essential post-error information.
# Todo/Fixme: the connection number default value can be designed
#             by WL7206.
#
# ==== Usage ====
#
# --let $gcs_error_connection_number= value
# --source include/rpl_gcs_error_out.inc
#
--let $_cnt= 1
while ($_cnt <= $gcs_error_connection_number)
{
  --connection server$_cnt

  --query_vertical select * from performance_schema.replication_connection_status
  --query_vertical select * from performance_schema.replication_connection_nodes

  --inc $_cnt
}
