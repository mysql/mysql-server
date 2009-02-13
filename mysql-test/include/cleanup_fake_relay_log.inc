# ==== Purpose ====
#
# Clean up files create by setup_fake_relay_log.inc.
#
# ==== Usage ====
#
# See setup_fake_relay_log.inc

--echo Cleaning up after setup_fake_relay_log.inc

# Remove files.
remove_file $_fake_relay_log;
remove_file $_fake_relay_index;
--disable_query_log
eval SET @@global.relay_log_purge= $_fake_relay_log_purge;
--enable_query_log
