# ==== Purpose ====
#
# Clean up replication configuration after using a 2ch
# setup.
# We need to explicitly reset the IGNORE_SERVER_IDS parameters
# on all Servers to avoid testcase check errors.
#
# ==== Usage ====
#
#   [--let $rpl_debug= 1]
#   --source suite/ndb_rpl/ndb_rpl_init_source_replica_2ch_end.inc
#
# Parameters:
#   $rpl_debug
#     See include/rpl/init_source_replica.inc

--source include/rpl/stop_all_replicas.inc
--connection master
CHANGE REPLICATION SOURCE TO IGNORE_SERVER_IDS= ();
--connection master1
CHANGE REPLICATION SOURCE TO IGNORE_SERVER_IDS= ();
--connection slave
CHANGE REPLICATION SOURCE TO IGNORE_SERVER_IDS= ();
--connection slave1
CHANGE REPLICATION SOURCE TO IGNORE_SERVER_IDS= ();
--source include/rpl/start_all_replicas.inc

--source include/rpl/deinit.inc