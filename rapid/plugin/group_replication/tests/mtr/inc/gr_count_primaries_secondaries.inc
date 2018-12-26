# ==== Purpose ====
#
# Counts the number of servers with role PRIMARY and
# role SECONDARY in the group.
#
# Result is placed on vars:
# - $group_replication_count_primaries_out_var
# - $group_replication_count_secondaries_out_var
#
# It relies on $rpl_server_count to iterate through
# the group by connecting to the different servers
# and counting them.
#
# The current connection is preserved.
#
# ==== Usage ====
#
# --source ../inc/gr_count_primaries_secondaries.inc
#
# --echo PRIMARIES:      $group_replication_count_primaries_out_var
# --echo SECONDARIES:    $group_replication_count_secondaries_out_var
# --echo PRIMARY SERVER: $group_replication_primary_server_connection_var

--let $include_filename= gr_count_primaries_secondaries.inc
--source include/begin_include_file.inc

# save the connection to reinstate it later
--let $_saved_current_connection= $CURRENT_CONNECTION

--let $group_replication_count_primaries_out_var= 0
--let $group_replication_count_secondaries_out_var= 0

--let $_i= $rpl_server_count
while ($_i)
{
  # connect to the server
  --let $rpl_connection_name=server$_i
  --source include/rpl_connection.inc

  let $_has_no_errors= `SELECT count(*) FROM performance_schema.replication_group_members
         WHERE MEMBER_ID = @@server_uuid AND
         MEMBER_STATE != "ERROR"`;

  let $_is_primary= `SELECT COUNT(*)=1 FROM performance_schema.global_status
                            WHERE VARIABLE_NAME = "group_replication_primary_member" AND
                            VARIABLE_VALUE = @@server_uuid`;

  let $_is_secondary= `SELECT COUNT(*)=1 FROM performance_schema.global_status
                            WHERE VARIABLE_NAME = "group_replication_primary_member" AND
                            VARIABLE_VALUE != @@server_uuid AND
                            VARIABLE_VALUE != ""`;

  if ($_has_no_errors)
  {
    if ($_is_primary)
    {
      --inc $group_replication_count_primaries_out_var
      --let $group_replication_primary_server_connection_var= server$_i
    }

    if ($_is_secondary)
    {
      --inc $group_replication_count_secondaries_out_var
    }
  }

  --dec $_i
}

--let $rpl_connection_name=$_saved_current_connection
--source include/rpl_connection.inc

--let $include_filename= group_replication_wait_until_role.inc
--source include/end_include_file.inc
