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
#   --source suite/ndb_rpl/ndb_master-slave_2ch_end.inc
#
# Parameters:
#   $rpl_debug
#     See include/master-slave.inc

--source include/rpl_stop_slaves.inc
--connection master
CHANGE MASTER TO IGNORE_SERVER_IDS= ();
--connection master1
CHANGE MASTER TO IGNORE_SERVER_IDS= ();
--connection slave
CHANGE MASTER TO IGNORE_SERVER_IDS= ();
--connection slave1
CHANGE MASTER TO IGNORE_SERVER_IDS= ();
--source include/rpl_start_slaves.inc

--source include/rpl_end.inc