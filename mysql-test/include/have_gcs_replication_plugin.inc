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
