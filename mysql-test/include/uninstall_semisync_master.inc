# Uninstall semisync master plugin
# Usage:
# --source include/uninstall_semisync_master.inc

--let $include_filename= uninstall_semisync_master.inc
--source include/begin_include_file.inc

--disable_query_log
--disable_warnings
UNINSTALL PLUGIN rpl_semi_sync_master;

--source include/end_include_file.inc
