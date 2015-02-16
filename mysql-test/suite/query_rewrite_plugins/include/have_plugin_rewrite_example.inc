#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip Rewrite example plugin requires dynamic loading
}

if (`SELECT $PS_PROTOCOL + $SP_PROTOCOL + $CURSOR_PROTOCOL +
    $VIEW_PROTOCOL + $OPT_TRACE_PROTOCOL + $EXPLAIN_PROTOCOL +
    $JSON_EXPLAIN_PROTOCOL > 0`)
{
   --skip Need normal protocol
}

#
# Check if the variable REWRITE_EXAMPLE is set
#
if (!$REWRITE_EXAMPLE) {
  --skip Rewrite example plugin requires the environment variable \$REWRITE_EXAMPLE to be set (normally done by mtr)
}

#
# Check if --plugin-dir was setup for exampledb
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$REWRITE_EXAMPLE_OPT/'`) {
  --skip Rewrite Example plugin requires that --plugin-dir is set to the example plugin dir (either the .opt file does not contain \$REWRITE_EXAMPLE_OPT or another plugin is in use)
}
enable_query_log;
