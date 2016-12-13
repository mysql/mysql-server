# ==== Purpose ====
#
# Finds a server with role PRIMARY.
#
# Result is placed on var:
# - $group_replication_primary_connection_out_var
# - $group_replication_found_primary_out_var
#
# It relies on $rpl_server_count to iterate through
# the group by connecting to the different servers
# and counting them.
#
# This iterates over all servers in the group every
# second until a timeout ocurrs or a primary is found.
#
# The current connection is preserved.
#
# ==== Usage ====
#
# --source ../inc/gr_find_a_primary.inc
# if ($group_replication_found_primary_out_var)
# {
#   --let $rpl_connection_name= $group_replication_primary_connection_out_var
#   --source include/rpl_connection.inc
#   # now on primary
# }

--let $include_filename= gr_find_a_primary.inc
--source include/begin_include_file.inc

# save the connection to reinstate it later
--let $_saved_current_connection= $CURRENT_CONNECTION

--let $group_replication_found_primary_out_var= 0
--let $group_replication_primary_connection_out_var=

# Let's wait 1 second between each try
--let $_retry_find_primary= $wait_timeout
while ($_retry_find_primary)
{
  --let $_i_find_primary= $rpl_server_count
  while ($_i_find_primary>0)
  {
    # connect to the server
    --let $rpl_connection_name=server$_i_find_primary
    --source include/rpl_connection.inc

    let $_is_primary= `SELECT COUNT(*)=1 FROM performance_schema.global_status
                              WHERE VARIABLE_NAME = "group_replication_primary_member" AND
                              VARIABLE_VALUE = @@server_uuid`;

    if ($_is_primary)
    {
      --let $group_replication_found_primary_out_var= 1
      --let $group_replication_primary_connection_out_var= server$_i_find_primary
      --let $_i_find_primary= 0
    }

    --dec $_i_find_primary
  }

  if (!$group_replication_found_primary_out_var)
  {
    --dec $_retry_find_primary
    --sleep 1
  }

  if ($group_replication_found_primary_out_var)
  {
    --let $_retry_find_primary= 0
  }
}

--let $rpl_connection_name=$_saved_current_connection
--source include/rpl_connection.inc

--let $include_filename= group_replication_wait_until_role.inc
--source include/end_include_file.inc
