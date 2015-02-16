# Uninstall Replication Observers Example plugin
# Usage:
# --source include/uninstall_replication_observers_example.inc

--let $include_filename= uninstall_replication_observers_example.inc
--source include/begin_include_file.inc

--disable_query_log

--disable_warnings
UNINSTALL PLUGIN replication_observers_example;
--enable_warnings

--source include/end_include_file.inc
