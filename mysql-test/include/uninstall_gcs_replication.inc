--let $include_filename= uninstall_gcs_replication.inc
--source include/begin_include_file.inc

# Uninstall gcs_replication_plugin
--disable_warnings
UNINSTALL PLUGIN gcs_replication_plugin;
--enable_warnings
--source include/end_include_file.inc
