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
  --skip Example plugin requires the environment variable \$EXAMPLE_PLUGIN to be set (normally done by mtr)
}

#
# Check if --plugin-dir was setup for exampledb
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$EXAMPLE_PLUGIN_OPT/'`) {
  --skip Example plugin requires that --plugin-dir is set to the example plugin dir (either the .opt file does not contain \$EXAMPLE_PLUGIN_OPT or another plugin is in use)
}
enable_query_log;
