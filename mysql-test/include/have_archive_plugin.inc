disable_query_log;
if (`select plugin_library IS NULL from information_schema.plugins where plugin_name LIKE '%archive%'`)
{
  --skip archive plugin not available
}

if (`SELECT @@plugin_dir != '$ARCHIVE_PLUGIN_DIR'`) {
  --skip Archive plugin requires that --plugin-dir is set to the archive plugin dir (either the .opt file does not contain \$ARCHIVE_PLUGIN_OPT or another plugin is in use)
}
enable_query_log;
