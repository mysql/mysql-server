--let $include_filename= have_replication_observers_example_plugin.inc
--source include/begin_include_file.inc

--disable_query_log
#
# Check if server has support for loading plugins
#
if (`SELECT @@have_dynamic_loading != 'YES'`) {
  --skip observers example plugin requires dynamic loading
}

#
# Check if the variable RPL_OBS_EXAMPLE is set
#
if (!$RPL_OBS_EXAMPLE) {
  --skip observers example plugin requires the environment variable \$RPL_OBS_EXAMPLE to be set (normally done by mtr)
}
--enable_query_log

#
# Check if --plugin-dir was setup for replication observers
#
if (`SELECT CONCAT('--plugin-dir=', REPLACE(@@plugin_dir, '\\\\', '/')) != '$RPL_OBS_EXAMPLE_OPT/'`) {
  --skip Replication Observers plugin requires that --plugin-dir is set to the Replication Observers dir (either the .opt file does not contain \$RPL_OBS_EXAMPLE_OPT or another plugin is in use)
}

--let $include_filename= have_replication_observers_example_plugin.inc
--source include/end_include_file.inc

