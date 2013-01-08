# ==== Purpose ====
#
# Configure two servers to be replication master and slave.
#
# ==== Usage ====
#
#   [--let $rpl_server_count= N]
#   [--let $rpl_check_server_ids= 1]
#   [--let $rpl_skip_reset_master_and_slave= 1]
#   [--let $rpl_skip_change_master= 1]
#   [--let $rpl_skip_start_slave= 1]
#   [--let $rpl_debug= 1]
#   [--let $slave_timeout= NUMBER]
#   [--let $use_gtids= 1]
#   --source include/master-slave.inc
#
# Parameters:
#   $rpl_check_server_ids, $rpl_skip_reset_master_and_slave,
#   $rpl_skip_change_master, $rpl_skip_start_slave, $rpl_debug,
#   $slave_timeout, $use_gtids
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


--let $rpl_topology= 1->2
--source include/rpl_init.inc

--source include/rpl_default_connections.inc


--let $include_filename= master-slave.inc
--source include/end_include_file.inc


# Set the default connection to 'master'. Do this after
# end_include_file.inc, so that it gets printed to the query log.
--let $rpl_connection_name= master
--source include/rpl_connection.inc
