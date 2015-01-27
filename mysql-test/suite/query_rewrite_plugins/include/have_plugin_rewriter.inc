source include/not_embedded.inc;


if (`SELECT $PS_PROTOCOL + $SP_PROTOCOL + $CURSOR_PROTOCOL +
    $VIEW_PROTOCOL + $OPT_TRACE_PROTOCOL + $EXPLAIN_PROTOCOL +
    $JSON_EXPLAIN_PROTOCOL > 0`)
{
   --skip Need normal protocol
}

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
