--let $include_filename= uninstall_group_replication_plugin.inc
--source include/begin_include_file.inc

# Uninstall group replication plugin
--disable_query_log
--disable_warnings
UNINSTALL PLUGIN group_replication;
--enable_warnings
--enable_query_log
--source include/end_include_file.inc
