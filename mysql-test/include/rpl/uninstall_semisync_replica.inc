# Uninstall semisync slave plugin
# Usage:
# --source include/rpl/uninstall_semisync_replica.inc

--let $include_filename= rpl/uninstall_semisync_replica.inc
--source include/begin_include_file.inc

--disable_query_log

--source include/rpl/stop_receiver.inc

--disable_warnings
UNINSTALL PLUGIN rpl_semi_sync_replica;
--enable_warnings

--source include/rpl/start_receiver.inc

--source include/end_include_file.inc
