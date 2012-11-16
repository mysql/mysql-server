disable_query_log;
#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip Null audit plugin requires dynamic loading
}

#
# Check if the variable AUDIT_NULL is set
#
if (!$AUDIT_NULL) {
  --skip Audit_null plugin requires the environment variable \$AUDIT_NULL to be set (normally done by mtr)
}

#
# Check if --plugin-dir was setup for null_audit db
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$AUDIT_NULL_OPT/'`) {
  --skip null audit plugin requires that --plugin-dir is set to the null audit plugin dir (either the .opt file does not contain \$AUDIT_NULL_OPT or another plugin is in use)
}
enable_query_log;
