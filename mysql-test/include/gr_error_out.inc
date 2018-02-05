# ==== Purpose ====
#
# Loop through a number of existing server connection to print out
# essential post-error information.
#
# ==== Usage ====
#
# --let $group_replication_error_connection_number= value
# --source include/rpl_gr_error_out.inc
#
--let $_cnt= 1
while ($_cnt <= $group_replication_error_connection_number)
{
  --connection server$_cnt

  --query_vertical select * from performance_schema.replication_connection_status
  --query_vertical select * from performance_schema.replication_group_members

  --inc $_cnt
}
