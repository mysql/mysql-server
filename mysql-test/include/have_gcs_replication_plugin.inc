--let $include_filename= have_gcs_replication_plugin.inc
--source include/begin_include_file.inc

#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip Requires dynamic loading
}

#
# Check if the variable GCS_REPLICATION_PLUGIN is set
#
if (!$GCS_RPL_PLUGIN)
{
  skip Need GCS_REPLICATION plugin;
}

#
# Check if --plugin-dir was setup for GCS REPLICATION
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/'))!='$GCS_RPL_PLUGIN_OPT/'`) {
  --skip GCS plugin requires that --plugin-dir is set to the gcs_replication plugin dir (either the .opt file does not contain \$GCS_RPL_PLUGIN_OPT or another plugin is in use)
}

# Allow $SERVER_MYPORT_1 as alias for $MASTER_MYPORT
# necessary for the rpl_gcs_default_connections.inc file.
if (!$SERVER_MYPORT_1)
{
  --let SERVER_MYPORT_1= $MASTER_MYPORT
}

# Allow $SERVER_MYPORT_2 as alias for $SLAVE_MYPORT
# necessary for the rpl_gcs_default_connections.inc file.
if (!$SERVER_MYPORT_2)
{
  --let SERVER_MYPORT_2= $SLAVE_MYPORT
}

--source include/rpl_gcs_default_connections.inc

--let $include_filename= have_gcs_replication_plugin.inc
--source include/end_include_file.inc
