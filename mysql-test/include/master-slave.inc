# ==== Purpose ====
#
# Configure two servers to be replication master and slave.
#
# ==== Usage ====
#
#   [--let $rpl_server_count= N]
#   [--let $rpl_extra_connections_per_server= 1]
#   [--let $rpl_check_server_ids= 1]
#   [--let $rpl_skip_reset_master_and_slave= 1]
#   [--let $rpl_skip_change_master= 1]
#   [--let $rpl_skip_start_slave= 1]
#   [--let $rpl_debug= 1]
#   [--let $slave_timeout= NUMBER]
#   [--let $use_gtids= 1]
#   [--let $rpl_gtid_utils= 1]
#   [--let $rpl_set_gtid_next_anonymous_in_new_connection= 1]
#   [--let $rpl_skip_group_replication_start= 1]
#   [--let $rpl_privilege_checks_user= 2:'user'@'host' | 1:NULL]
#   [--let $rpl_privilege_checks_user_additional_grants= CREATE,INSERT,SELECT]
#   [--let $rpl_require_row_format= *:0,1:0,2:1]
#   [--let $rpl_require_table_primary_key_check= *:STREAM,1:ON,2:OFF,3:GENERATE]
#   [--let $rpl_gtid_only= *:0,1:0,2:1]
#   [--let $rpl_shallow_reset_slave= 1]
#   --source include/master-slave.inc
#
# Parameters:
#   $rpl_extra_connections_per_server, $rpl_check_server_ids,
#   $rpl_skip_reset_master_and_slave, $rpl_skip_change_master,
#   $rpl_skip_start_slave, $rpl_debug, $slave_timeout, $use_gtids,
#   $rpl_gtid_utils, $rpl_set_gtid_next_anonymous_in_new_connection,
#   $rpl_skip_group_replication_start, $rpl_privilege_checks_user,
#   $rpl_privilege_checks_user_additional_grants, $rpl_require_row_format,
#   $rpl_require_table_primary_key_check $rpl_shallow_reset_slave
#     See include/rpl_init.inc
#
#   $rpl_server_count
#     By default, two servers are configured. You can configure more
#     servers (servers 3, 4, etc are neither masters nor slaves) by
#     setting this variable. See also include/rpl_init.inc


--let $include_filename= master-slave.inc
if ($rpl_server_count)
{
  --let $include_filename= master-slave.inc [rpl_server_count=$rpl_server_count]
}
--source include/begin_include_file.inc

--source include/rpl_group_replication_plugin_installed.inc
if ($group_replication_plugin_installed)
{
  --let $wait_for_executed_gtid_set= 1
  --let $rpl_group_replication= 1
}

--let $rpl_topology= 1->2
--source include/rpl_init.inc

--source include/rpl_default_connections.inc


--let $include_filename= master-slave.inc
--source include/end_include_file.inc


# Set the default connection to 'master'. Do this after
# end_include_file.inc, so that it gets printed to the query log.
--let $rpl_connection_name= master
--source include/rpl_connection.inc
