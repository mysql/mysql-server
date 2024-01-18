# ==== Purpose ====
#
# Configure N servers to be members of a group.
#
# ==== Usage ====
#
#   [--let $rpl_debug= 1]
#   [--let $rpl_server_count= N]
#   [--let $rpl_skip_group_replication_start= 1]
#   [--let $rpl_group_replication_single_primary_mode= 1]
#   --source include/group_replication.inc
#
# Parameters:
#   $rpl_server_count
#     See include/rpl/init_source_replica.inc
#
#   $rpl_skip_group_replication_start
#     See include/rpl/init.inc
#
#   $rpl_group_replication_single_primary_mode
#     Sets configuration for single primary mode

--let $include_filename= group_replication.inc
if ($rpl_server_count)
{
  --let $include_filename= group_replication.inc [rpl_server_count=$rpl_server_count]
}
--source include/begin_include_file.inc

# Setup and start group.
--source include/rpl/init_source_replica.inc

# Reset group_replication_bootstrap_group option on all servers.
--source include/gr_clear_bootstrap_group.inc

# Set single primary mode if needed
if ($rpl_group_replication_single_primary_mode)
{
  --let $reset_single_primary_mode= 0
  --source include/gr_single_primary_mode.inc
}

--let $include_filename= group_replication.inc
--source include/end_include_file.inc

# Set the default connection to 'server1'. Do this after
# end_include_file.inc, so that it gets printed to the query log.
--let $rpl_connection_name= server1
--source include/connection.inc
