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
#   --source ../inc/group_replication.inc
#
# Parameters:
#   $rpl_server_count
#     See include/master-slave.inc
#
#   $rpl_skip_group_replication_start
#     See include/rpl_init.inc
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
--source include/master-slave.inc

# Disable rpl debug that is added automatically on wait conditions
# failures since it does only contain asynchronous replication
# information and hides GR debug information.
--let $_show_rpl_debug_info_previous= $show_rpl_debug_info
--let $show_rpl_debug_info= 0

# Reset group_replication_bootstrap_group option on all servers.
--source ../inc/gr_clear_bootstrap_group.inc

# Set single primary mode if needed
if ($rpl_group_replication_single_primary_mode)
{
  --let $reset_single_primary_mode= 0
  --source ../inc/gr_single_primary_mode.inc
}

--let $include_filename= group_replication.inc
--source include/end_include_file.inc

# Set the default connection to 'server1'. Do this after
# end_include_file.inc, so that it gets printed to the query log.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
