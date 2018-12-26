

# Test requires: sp-protocol/ps-protocol/view-protocol/cursor-protocol disabled
--source include/no_protocol.inc
--source include/no_opt_trace_protocol.inc
--source include/no_explain_protocol.inc
--source include/no_json_explain_protocol.inc

if ( `SELECT @@have_dynamic_loading != 'YES'` ) {
  skip Rewriter plugin requires dynamic loading;
}

if ( !$REWRITER ) {
  skip Rewriter plugin requires the environment variable \$REWRITER to be set (normally done by mtr);
}

if ( `SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) !=
     '$REWRITER_OPT/'` ) {
  skip Rewriter plugin requires that --plugin-dir is set to the tree plugin dir (either the .opt file does not contain \$REWRITER_OPT or another plugin is in use);
}
enable_query_log;
