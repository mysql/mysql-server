# ==== Purpose ====
#
# Clean up files create by setup_fake_relay_log.inc.
#
# ==== Usage ====
#
# See setup_fake_relay_log.inc

--echo Cleaning up after setup_fake_relay_log.inc

--disable_query_log
--disable_warnings
STOP SLAVE SQL_THREAD;
RESET SLAVE;
eval SET @@global.relay_log_purge= $_fake_relay_log_purge;
--enable_warnings
--enable_query_log
