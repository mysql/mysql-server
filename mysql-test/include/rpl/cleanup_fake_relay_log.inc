# ==== Purpose ====
#
# Clean up files created by setup_fake_relay_log.inc.
#
# ==== Usage ====
#
# See setup_fake_relay_log.inc

--let $include_filename= rpl/cleanup_fake_relay_log.inc
--source include/begin_include_file.inc


--disable_query_log
RESET REPLICA;

# Assert that the fake relay log files are gone (RESET REPLICA should
# have removed them).
--let $file_does_not_exist= $_fake_relay_log
--source include/file_does_not_exist.inc

# Revert variables.
eval SET @@global.relay_log_purge= $_fake_relay_log_purge;

eval CHANGE REPLICATION SOURCE TO SOURCE_HOST = '$_fake_old_source_host';


--let $include_filename= rpl/cleanup_fake_relay_log.inc
--source include/end_include_file.inc
