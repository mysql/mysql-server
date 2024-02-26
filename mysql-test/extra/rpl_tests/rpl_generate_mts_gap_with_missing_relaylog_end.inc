# ==== Purpose ====
#
# Clean up what
# extra/rpl_tests/rpl_generate_mts_gap_with_missing_relaylog_begin.inc created.
#

--echo # Sourcing extra/rpl_tests/rpl_generate_mts_gap_with_missing_relaylog_end.inc

--source include/rpl_connection_master.inc
DROP DATABASE d1;
DROP DATABASE d2;
--source include/sync_slave_sql_with_master.inc
--remove_file $MYSQLD_DATADIR/relay.backup
--source include/rpl_reset.inc
