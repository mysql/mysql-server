# ==== Purpose ====
#
# Check that Group Replication plugin is installed and that test
# case server configuration fulfills Group Replication requirements.
#
# Also it configures:
#   1) group_replication_group_name, if not configured previously.
#   2) client connections to all group servers.
#   3) XCom configuration.
#
# ==== Usage ====
#
#   [--let $group_replication_group_name= UUID]
#   [--let $rpl_skip_group_replication_configuration= 1]
#   --source include/have_group_replication_plugin.inc
#
#   Parameters:
#
#   $group_replication_group_name
#     By default, this script generates a random group name to be
#     used on the test that sources it.
#     If this parameter is provided, its value will be used as
#     group name.
#
#   $skip_group_replication_configuration
#     By default, this script configures XCom on all servers.
#     If this parameter is provided, the XCom configuration setup
#     is skipped.
#
#   $skip_recovery_configuration
#     By default, this script will configure the servers with a default value
#     for a user_name and password which will be used for distributed
#     recovery. However, if this parameter is provided, the recovery
#     configuration is skipped.
#

--let $include_filename= have_group_replication_plugin.inc
--let $_previous_include_silent= $include_silent
--let $include_silent= 1
--source include/begin_include_file.inc

# Check server configuration.
--source include/have_group_replication_plugin_base.inc

# Allow $SERVER_MYPORT_1 as alias for $MASTER_MYPORT
# necessary for the rpl_group_replication_default_connections.inc file.
if (!$SERVER_MYPORT_1)
{
  --let SERVER_MYPORT_1= $MASTER_MYPORT
}

# Allow $SERVER_MYPORT_2 as alias for $SLAVE_MYPORT
# necessary for the rpl_group_replication_default_connections.inc file.
if (!$SERVER_MYPORT_2)
{
  --let SERVER_MYPORT_2= $SLAVE_MYPORT
}

--source include/rpl_group_replication_default_connections.inc

# Generate group name if not defined yet.
if ($group_replication_group_name == '')
{
  --let $group_replication_group_name= `SELECT UUID()`
}

if (!$rpl_skip_group_replication_configuration)
{
  # Configure XCom.
  --source include/gr_configuration.inc
}

--let $include_filename= have_group_replication_plugin.inc
--let $include_silent= $_previous_include_silent
--source include/end_include_file.inc
