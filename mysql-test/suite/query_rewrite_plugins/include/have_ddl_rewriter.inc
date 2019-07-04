#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip DDL rewriter plugin requires dynamic loading
}

# Test requires: sp-protocol/ps-protocol/view-protocol/cursor-protocol disabled
--source include/no_protocol.inc
--source include/no_opt_trace_protocol.inc
--source include/no_explain_protocol.inc
--source include/no_json_explain_protocol.inc

#
# Check if the variable DDL_REWRITER is set
#
if (!$DDL_REWRITER) {
  --skip DDL rewriter plugin requires the environment variable \$DDL_REWRITER to be set (normally done by mtr)
}

#
# Check if --plugin-dir was setup
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$DDL_REWRITER_OPT/'`) {
  --skip DDL rewriter plugin requires that --plugin-dir is set to the DDL rewriter plugin dir (either the .opt file does not contain \$DDL_REWRITER_OPT or another plugin is in use)
}
enable_query_log;
