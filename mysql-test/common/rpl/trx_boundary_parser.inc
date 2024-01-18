# ==== Purpose ====
#
# This include will insert some data into a table at the master varying the
# debug sync point at slave that will be used to stop the IO thread in the
# middle of transaction event stream (trying to let partial transactions in
# the relay log).
#
# It will do this task (insert some data) twice.
#
# The first time with the SQL thread stopped, letting the IO thread do its job
# until all data is replicated, starting the SQL only at the end of the test.
#
# The second time, the SQL thread will be running all the time, syncing on each
# step of the test.
#
# ==== Usage ====
#
# [--let $storage_engine= InnoDB | MyISAM]
# --source common/rpl/trx_boundary_parser.inc
#
# Parameters:
#   $storage_engine
#     The storage engine that will be used in the CREATE TABLE statement.
#     If not specified, InnoDB will be used.
#

if ( `SELECT '$storage_engine' != '' AND UPPER('$storage_engine') <> 'INNODB' AND UPPER('$storage_engine') <> 'MYISAM'` )
{
  --die ERROR IN TEST: invalid value for mysqltest variable 'storage_engine': $storage_engine
}

--echo ## Running the test with the SQL thread stopped
--source include/rpl/connection_replica.inc
--source include/rpl/stop_applier.inc
--source common/rpl/trx_boundary_parser_all_steps.inc

--echo ## Starting and syncing the SQL thread before next round
--source include/rpl/connection_replica.inc
--source include/rpl/start_applier.inc
--source include/rpl/connection_source.inc
--source include/rpl/sync_to_replica.inc

--echo ## Running the test with the SQL thread started
--source common/rpl/trx_boundary_parser_all_steps.inc
