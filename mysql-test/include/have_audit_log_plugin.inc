#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip audit_log plugin requires dynamic loading
}

#
# Check if the variable AUDIT_LOG_PLUGIN is set
#
if (!$AUDIT_LOG_PLUGIN) {
  --skip audit_log plugin requires the environment variable \$AUDIT_LOG_PLUGIN to be set (normally done by mtr)
}

#
# Check if --plugin-dir was setup for audit_log 
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$AUDIT_LOG_PLUGIN_OPT/'`) {
  --skip audit_log plugin requires that --plugin-dir is set to the audit_log plugin dir (either the .opt file does not contain \$AUDIT_LOG_PLUGIN_OPT or another plugin is in use)
}

