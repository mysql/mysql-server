# ==== Purpose ====
#
# This include file simulates the scenario of missing relay logs.  This is
# done by creating gaps in the execution of a Multi threaded slave, killing
# the slave and removing the relay log that is required to fill the gaps.
#
# ==== Implementation ====
#
# Creates two databases d1, d2 and create tables d1.t, d2.t in each database.
# Execute FLUSH LOGS on master so that relay logs are flushed till this point.
#
# On slave, copy the newly created relay log to a backup file. Begin a new
# transaction and do a local insert on d2.t table such that any inserts on d2.t
# table from master will be blocked. Make other worker to over take the blocked
# worker and commit a transaction for example here T5 transaction. This will
# generate gaps.
#
# Crash the slave server at this point and delete the relay log that has the
# above insert operations and copy the backup relay log in its place. This
# simulates missing relay log scenario.
#
# ==== Note ====
#
# 1. Source common/rpl/generate_mta_gap_with_missing_relaylog_end.inc
#    in the end of test to perform cleanup of things created by this test.
#
# ==== References ====
#
# Bug#28830834: MTS NOT REPLICATION CRASH-SAFE WITH GTID AND ALL THE RIGHT PARAMETERS.
#

--echo # Sourcing common/rpl/generate_mta_gap_with_missing_relaylog_begin.inc

--source include/rpl/connection_replica.inc
let $MYSQLD_DATADIR= `select @@datadir`;
--source include/rpl/stop_replica.inc
SET @@GLOBAL.replica_parallel_workers=2;
SET @@GLOBAL.replica_preserve_commit_order=OFF;
--source include/rpl/start_replica.inc

--source include/rpl/connection_source.inc
CREATE DATABASE d1;
CREATE DATABASE d2;
CREATE TABLE d1.t (a INT PRIMARY KEY, name TEXT) ENGINE=INNODB;
CREATE TABLE d2.t (a INT PRIMARY KEY, name TEXT) ENGINE=INNODB;
# To ensure one DML in relay log file after rotate events.
FLUSH LOGS;
INSERT INTO d1.t VALUES (1, 'T1');
--source include/rpl/sync_to_replica.inc

# Create back up for newly created relay log
--let $MYSQLD_DATADIR= `SELECT @@datadir`
--let $relay_log_file= query_get_value(SHOW REPLICA STATUS, Relay_Log_File, 1)
--copy_file $MYSQLD_DATADIR/$relay_log_file $MYSQLD_DATADIR/relay.backup

BEGIN;
INSERT INTO d2.t VALUES (2, 'Slave local'); # Hold T3

--source include/rpl/connection_source1.inc

# Begin T4 so it is parallel using DATABASE | LOGICAL CLOCK (WRITESET) | LOGICAL CLOCK (COMMIT ORDER)
BEGIN;
INSERT INTO d1.t VALUES (2, 'T4');

--source include/rpl/connection_source.inc

INSERT INTO d2.t VALUES (1, 'T2');
INSERT INTO d2.t VALUES (2, 'T3'); # This will be a gap when executed on the replica

--source include/rpl/connection_source1.inc
COMMIT;

--source include/rpl/connection_replica1.inc
--let $table=d2.t
--let $count=1
--source include/wait_until_rows_count.inc

--let $table=d1.t
--let $count=2
--source include/wait_until_rows_count.inc

# Wait for coordinator to populate worker's queues.
--let $show_statement= SHOW PROCESSLIST
--let $field= State
--let $condition= = 'Replica has read all relay log; waiting for more updates'
--source include/wait_show_condition.inc

# Kill the replica server
--let $rpl_server_number= 2
--let $rpl_force_stop=1
--source include/rpl/stop_server.inc

# Wait until the relay log fle is removed
--let active_relaylog= $MYSQLD_DATADIR/$relay_log_file
perl;
my $filetodelete = "$ENV{'active_relaylog'}";
while (-e $filetodelete) {
  unlink $filetodelete;
  sleep 1;
}
EOF

--copy_file $MYSQLD_DATADIR/relay.backup $MYSQLD_DATADIR/$relay_log_file
