# ==== Purpose ====
#
# Clean up what
# common/rpl/generate_mta_gap_with_missing_relaylog_begin.inc created.
#

--echo # Sourcing common/rpl/generate_mta_gap_with_missing_relaylog_end.inc

--source include/rpl/connection_source.inc
DROP DATABASE d1;
DROP DATABASE d2;
--source include/rpl/sync_to_replica.inc
--remove_file $MYSQLD_DATADIR/relay.backup
--source include/rpl/reset.inc
