#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip Requires dynamic loading
}

#
# Check if the variable SEMISYNC_SOURCE_PLUGIN is set
#
if (!$SEMISYNC_SOURCE_PLUGIN)
{
  skip Need semisync plugins;
}

#
# Check if --plugin-dir was setup for semisync
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$SEMISYNC_PLUGIN_OPT/'`) {
  --skip SEMISYNC plugin requires that --plugin-dir is set to the semisync plugin dir (either the .opt file does not contain \$SEMISYNC_PLUGIN_OPT or another plugin is in use)
}
