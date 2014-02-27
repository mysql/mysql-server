#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip Example plugin requires dynamic loading
}

#
# Check if the variable EXAMPLE_PLUGIN is set
#
if (!$EXAMPLE_PLUGIN) {
  --skip Could not find library for example plugin, see the file plugin.defs
}

#
# Check if --plugin-dir was setup for exampledb
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$EXAMPLE_PLUGIN_OPT/'`) {
  --skip Example plugin requires that --plugin-dir is set to the example plugin dir
}
enable_query_log;
