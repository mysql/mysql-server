#
# Check if dynamic loading is supported
#
--require r/have_dynamic_loading.require
disable_query_log;
show variables like 'have_dynamic_loading';
enable_query_log;

#
# Check if the variable SEMISYNC_MASTER_PLUGIN is set
#
if (`select LENGTH('$SEMISYNC_MASTER_PLUGIN') = 0`)
{
  skip Need semisync plugins;
}

#
# Check if --plugin-dir was setup for semisync
#
if (`SELECT CONCAT('--plugin-dir=', @@plugin_dir) != '$SEMISYNC_PLUGIN_OPT'`) {
  --skip SEMISYNC plugin requires that --plugin-dir is set to the semisync plugin dir (either the .opt file does not contain \$SEMISYNC_PLUGIN_OPT or another plugin is in use)
}
