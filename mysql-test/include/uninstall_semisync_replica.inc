# Uninstall semisync slave plugin
# Usage:
# --source include/uninstall_semisync_replica.inc

--let $include_filename= uninstall_semisync_replica.inc
--source include/begin_include_file.inc

--disable_query_log

--source include/stop_slave_io.inc

--disable_warnings
UNINSTALL PLUGIN rpl_semi_sync_replica;
--enable_warnings

--source include/start_slave_io.inc

--source include/end_include_file.inc
