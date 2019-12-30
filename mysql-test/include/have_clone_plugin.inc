# Check the presence of the clone plugin

#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip clone plugin requires dynamic loading
}

#
# Check if the variable CLONE_PLUGIN is set
#
if (!$CLONE_PLUGIN) {
  --skip clone plugin requires the environment variable \$CLONE_PLUGIN to be set (normally done by mtr)
}

#
# Check if --plugin-dir was setup for plugins
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$CLONE_PLUGIN_OPT/'`) {
  --skip Clone plugin requires that --plugin-dir is set to the Clone plugin dir (either the .opt file does not contain \$CLONE_PLUGIN_OPT or another plugin is in use)
}
