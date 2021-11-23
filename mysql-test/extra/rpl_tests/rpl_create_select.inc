#########################################
# ==== Purpose ====
#
# Test crash and recovery when applying CREATE TABLE ... SELECT
# at slave. This test is run using both rpl.rpl_create_select and
# rpl.rpl_gtid_create_select.
#
# ==== Requirements ====
#
# R1 Check that the crash during CREATE TABLE ... SELECT at slave
#    causes table being created to be not seen after slave server
#    restart.
#
# R2 Verify that explicitly starting the slave thread should be able
#    to apply the binlog from last recovery point and then we should be
#    able to see the table.
#
# R3 Verify that executing CREATE TABLE ... SELECT with sql mode set
#    to ANSI gets replicated to slave. This test is added for Bug#33064062.
#
# ==== Implementation ====
#
# 1) Create table t0 on the master and insert a record.
# 2) Sync slave with master and verify that t0 is sync in slave.
# 3) Stop the slave.
# 4) Execute CREATE TABLE t1 AS SELECT * FROM t0; on master.
# 5) Connect to slave.
# 6) Enable the debug point to induce crash.
# 7) Start the slave, this would cause slave to crash.
# 8) Wait for slave server to completely stop.
# 9) Restart the server with slave thread stopped.
# 10) Set MASTER_LOG_POS to last log position from where logs to apply.
#     Set this only when GTID is off.
# 11) Start the slave thread.
# 12) Sync the master with the slave.
# 13) Verify that both t0 and t1 is present on master and slave.
# 14) Cleanup.
#
# ==== References ====
#
# WL#13355 Make CREATE TABLE...SELECT atomic and crash-safe
#

--source include/have_binlog_format_row.inc
--source include/have_debug.inc
--source include/not_valgrind.inc # Test takes too much time in Valgrind
source include/master-slave.inc;

--echo # Setup schema common to master and slave.
--source include/rpl_connection_master.inc
CREATE TABLE t0 (f1 INT);
INSERT INTO t0 VALUES (1);
INSERT INTO t0 VALUES (2);
INSERT INTO t0 VALUES (3);
INSERT INTO t0 VALUES (4);

--source include/sync_slave_sql_with_master.inc
--let $diff_tables=master:t0, slave:t0
--source include/diff_tables.inc

--echo # Stop the slave, so that we enable debug flag and apply binlog events
--echo # after master generates binlog events for CREATE TABLE ... SELECT.
STOP SLAVE;

--source include/rpl_connection_master.inc
--source include/save_master_pos.inc
CREATE TABLE t1 AS SELECT * FROM t0;
FLUSH LOGS;

--source include/rpl_connection_slave.inc

--echo # Crash slave server when applying row event (the INSERT)
SET global DEBUG='+d, crash_on_transactional_ddl_insert';
START SLAVE;

--echo # Wait for server to stop
--source include/expect_crash.inc
--enable_reconnect
--source include/wait_until_disconnected.inc

--echo # Restart the server.
--let $rpl_server_number= 2
--source include/rpl_start_server.inc
--disable_reconnect
# Debug point disappeared on restart

--let $assert_text= Make sure there is only table 't0' in schema 'test'.
let $assert_cond= [SELECT count(table_name) COUNT FROM
INFORMATION_SCHEMA.TABLES WHERE table_schema = \'test\', COUNT, 1] = 1;
--source include/assert.inc

--echo # Reset the master log position back to event that starts
--echo # CREATE TABLE ... SELECT and start the slave, without causing crash.
SELECT @@GLOBAL.GTID_MODE;
if ( `SELECT @@GLOBAL.GTID_MODE = "OFF"` ) 
{
  --replace_result $_saved_pos SAVED_POS
  --eval CHANGE REPLICATION SOURCE TO SOURCE_LOG_FILE='master-bin.000001', SOURCE_LOG_POS=$_saved_pos
}
--source include/start_slave.inc

--echo # Sync master and slave events.
--source include/rpl_connection_master.inc

--source include/sync_slave_sql_with_master.inc

--echo # Check that we have table 't1' now.
--let $diff_tables=master:t1, slave:t1
--source include/diff_tables.inc

--echo # Cleanup
--source include/rpl_connection_master.inc
DROP TABLE t0,t1;
--source include/sync_slave_sql_with_master.inc

--echo #
--echo # R3 Verify that executing CREATE TABLE ... SELECT with sql mode set
--echo #    to ANSI gets replicated to slave. This test is added for Bug#33064062.
--echo #

--source include/rpl_connection_master.inc
SET @orig_sql_mode_session= @@SESSION.sql_mode;
SET SESSION sql_mode='ANSI';
CREATE TABLE t1 AS SELECT 1;

--source include/sync_slave_sql_with_master.inc
--let $diff_tables=master:t1, slave:t1
--source include/diff_tables.inc

--source include/rpl_connection_master.inc
SET SESSION sql_mode= @orig_sql_mode_session;
DROP TABLE t1;
--source include/sync_slave_sql_with_master.inc

--source include/rpl_end.inc
