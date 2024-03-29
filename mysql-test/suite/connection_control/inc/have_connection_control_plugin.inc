disable_query_log;
#
# Check if server has support for loading plugin
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip The connection_control plugin requires dynamic loading
}

#
# Check if the variable CONNECTION_CONTROL_PLUGIN is set
#
if (!$CONNECTION_CONTROL_PLUGIN) {
  --skip The connection_control plugin requires the environment variable \$CONNECTION_CONTROL_PLUGIN to be set (normally done by mtr)
}

#
# Check if --plugin-dir was setup for null_audit_db
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$CONNECTION_CONTROL_PLUGIN_OPT/'`) {
  --skip The connection_control plugin requires that --plugin-dir is set to the connection_control plugin dir (either the .opt file does not contain \$CONNECTION_CONTROL_PLUGIN_OPT or another plugin is in use)
}
enable_query_log;
