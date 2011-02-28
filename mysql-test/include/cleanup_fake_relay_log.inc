# ==== Purpose ====
#
# Clean up files created by setup_fake_relay_log.inc.
#
# ==== Usage ====
#
# See setup_fake_relay_log.inc

--let $include_filename= cleanup_fake_relay_log.inc
--source include/begin_include_file.inc


--disable_query_log
RESET SLAVE;

# Assert that the fake relay log files are gone (RESET SLAVE should
# have removed them).
--let $file_does_not_exist= $_fake_relay_log
--source include/file_does_not_exist.inc

# Revert variables.
eval SET @@global.relay_log_purge= $_fake_relay_log_purge;

eval CHANGE MASTER TO MASTER_HOST = '$_fake_old_master_host';


--let $include_filename= cleanup_fake_relay_log.inc
--source include/end_include_file.inc
