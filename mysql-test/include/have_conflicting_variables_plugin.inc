#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip conflicting_variables plugin requires dynamic loading
}

#
# Check if the variable CONFLICTING_VARIABLES is set
#
if (!$CONFLICTING_VARIABLES) {
  --skip conflicting_variables plugin requires the environment variable \$CONFLICTING_VARIABLES to be set (normally done by mtr)
}

#
# Check if --plugin-dir was setup for conflicting_variables plugin
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$CONFLICTING_VARIABLES_OPT/'`) {
  --skip conflicting_variables plugin requires that --plugin-dir is set to the plugin dir (either the .opt file does not contain \$CONFLICTING_VARIABLES_OPT or another plugin is in use)
}
