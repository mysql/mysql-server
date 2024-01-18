--let $include_filename= rpl/uninstall_semisync.inc
--source include/begin_include_file.inc

--disable_query_log
--connection slave
--source include/rpl/stop_receiver.inc

# Uninstall rpl_semi_sync_replica first
--disable_warnings
UNINSTALL PLUGIN rpl_semi_sync_replica;

--connection master
# After BUG#17638477 fix, uninstallation of rpl_semi_sync_source
#  is not allowed when there are semi sync slaves. Hence kill
#  all dump threads before uninstalling it.
SET GLOBAL rpl_semi_sync_source_enabled = OFF;
--source include/rpl/stop_dump_threads.inc
UNINSTALL PLUGIN rpl_semi_sync_source;
--enable_warnings

--connection slave
--source include/rpl/start_receiver.inc
--enable_query_log

--source include/end_include_file.inc
