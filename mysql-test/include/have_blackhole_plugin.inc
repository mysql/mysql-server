disable_query_log;
if (`select plugin_library IS NULL from information_schema.plugins where plugin_name LIKE '%blackhole%'`)
{
  --skip blackhole plugin not available;
}

if (`SELECT @@plugin_dir != '$BLACKHOLE_PLUGIN_DIR'`) {
  --skip Blackhole plugin requires that --plugin-dir is set to the blackhole plugin dir (either the .opt file does not contain \$BLACKHOLE_PLUGIN_OPT or another plugin is in use)
}
enable_query_log;

