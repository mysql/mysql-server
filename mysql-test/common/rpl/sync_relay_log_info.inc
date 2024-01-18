# ==== Purpose ====
#
# This include will verify the update of the relay log info on table
# in a set of situations described at rpl_sync_relay_log_info.test
#
# ==== Usage ====
#
# [--let $storage_engine= InnoDB | MyISAM]
# --source common/rpl/sync_relay_log_info.inc
#
# Parameters:
#   $storage_engine
#     The storage engine that will be used in the CREATE TABLE statement.
#     If not specified, InnoDB will be used.
#

if (!$storage_engine)
{
  --let $_storage_engine= INNODB
}
if ($storage_engine)
{
  --let $_storage_engine= `SELECT UPPER('$storage_engine')`
}
if ( `SELECT '$_storage_engine' <> 'INNODB' AND '$_storage_engine' <> 'MYISAM'` )
{
  --die ERROR IN TEST: invalid value for mysqltest variable 'storage_engine': $storage_engine
}

# Sync slave SQL with master
--source include/rpl/connection_source.inc
--source include/rpl/sync_to_replica.inc

# Define test "header"
--let $_rrli_sync= `SELECT @@GLOBAL.sync_relay_log_info`
--let $_rrli_header= $_storage_engine, sync_rli=$_rrli_sync

# Initialize the sub test counter
--let $_rrli_counter= 0

# 1 - Stop the slave should always sync SQL thread positions
--source include/rpl/stop_replica.inc
--let $gtid_step_uuid= $master_uuid
--source include/rpl/gtid_step_reset.inc
--let $gtid_step_count= 0
--source common/rpl/sync_relay_log_info_assert_pos.inc

# Start the slave
--source include/rpl/start_replica.inc

# 2 - Simple DDL
--source include/rpl/connection_source.inc
--eval CREATE TABLE t1 (c1 int PRIMARY KEY) ENGINE=$storage_engine
--source include/rpl/sync_to_replica_received.inc
--let $gtid_step_count= 1
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 3 - Simple DML
--source include/rpl/connection_source.inc
INSERT INTO t1 VALUES (1);
--source include/rpl/sync_to_replica_received.inc
--let $gtid_step_count= 1
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 4 - The master has flushed the binary log
--source include/rpl/connection_source.inc
FLUSH LOCAL BINARY LOGS;
--source include/rpl/sync_to_replica_received.inc
--let $gtid_step_count= 0
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 5 - Simple DDL
--source include/rpl/connection_source.inc
DROP TABLE t1;
--source include/rpl/sync_to_replica_received.inc
--let $gtid_step_count= 1
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 6 - Stop the slave and restart only the SQL thread
--source include/rpl/stop_replica.inc
--source include/rpl/start_applier.inc
--let $gtid_step_count= 0
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 7 - The GTID of a simple DDL
--source include/rpl/connection_source.inc
--eval CREATE TABLE t1 (c1 int PRIMARY KEY) ENGINE=$storage_engine

# Hang the I/O thread after the GTID is received
--source include/rpl/connection_replica.inc
--let $rpl_after_received_events_action= hang
--let $rpl_event_count= 1
--let $rpl_count_only_event_type= Gtid
--let $rpl_continue_event_count_after_hanging= 0
--source include/rpl/receive_event_count.inc
--let $gtid_step_count= 0
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 8 - Flushed the relay log in the middle of the DDL
FLUSH LOCAL RELAY LOGS;
--let $gtid_step_count= 0
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 9 - The rest of the simple DDL
SET DEBUG_SYNC= 'now SIGNAL continue_after_queue_event';
--source include/rpl/connection_source.inc
--source include/rpl/sync_to_replica_received.inc
--let $gtid_step_count= 1
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 10 - Stop the slave and restart only the SQL thread
--source include/rpl/stop_replica.inc
--source include/rpl/start_applier.inc
--let $gtid_step_count= 0
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 11 - The GTID of a simple DML
--source include/rpl/connection_source.inc
INSERT INTO t1 VALUES (2);

# Hang the I/O thread after the GTID is received
--source include/rpl/connection_replica.inc
--let $rpl_after_received_events_action= hang
--let $rpl_event_count= 1
--let $rpl_count_only_event_type= Gtid
--let $rpl_continue_event_count_after_hanging= 0
--source include/rpl/receive_event_count.inc
--let $gtid_step_count= 0
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 12 - Flushed the relay log in the middle of the DML
FLUSH LOCAL RELAY LOGS;
--let $gtid_step_count= 0
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 13 - The rest of the simple DML
SET DEBUG_SYNC= 'now SIGNAL continue_after_queue_event';
--source include/rpl/connection_source.inc
--source include/rpl/sync_to_replica_received.inc
--let $gtid_step_count= 1
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 14 - Stop the slave and restart only the SQL thread
--source include/rpl/stop_replica.inc
--source include/rpl/start_applier.inc
--let $gtid_step_count= 0
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 15 - The GTID of a simple DML with a User var
--source include/rpl/connection_source.inc
SET @v=3;
INSERT INTO t1 VALUES (@v);

# Hang the I/O thread after the User var is received
--source include/rpl/connection_replica.inc
--let $rpl_after_received_events_action= hang
--let $rpl_event_count= 1
--let $rpl_count_only_event_type= User var
--let $rpl_continue_event_count_after_hanging= 0
--source include/rpl/receive_event_count.inc
--let $gtid_step_count= 0
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 16 - Flushed the relay log in the middle of the DML
FLUSH LOCAL RELAY LOGS;
--let $gtid_step_count= 0
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 17 - The rest of the simple DML
SET DEBUG_SYNC= 'now SIGNAL continue_after_queue_event';
--source include/rpl/connection_source.inc
--source include/rpl/sync_to_replica_received.inc
--let $gtid_step_count= 1
--source common/rpl/sync_relay_log_info_assert_pos.inc

# 18 - Simple DDL
--source include/rpl/connection_source.inc
DROP TABLE t1;
--source include/rpl/sync_to_replica_received.inc
--let $gtid_step_count= 1
--source common/rpl/sync_relay_log_info_assert_pos.inc

# The last part is only applicable for transactional tables
if ($_storage_engine == "INNODB")
{
  # 19 - Simple DDL
  --source include/rpl/connection_source.inc
  --eval CREATE TABLE t1 (c1 int PRIMARY KEY) ENGINE=$storage_engine
  --source include/rpl/sync_to_replica_received.inc
  --let $gtid_step_count= 1
  --source common/rpl/sync_relay_log_info_assert_pos.inc

  # 20 - Stop the slave and restart only the SQL thread
  --source include/rpl/stop_replica.inc
  --source include/rpl/start_applier.inc
  --let $gtid_step_count= 0
  --source common/rpl/sync_relay_log_info_assert_pos.inc

  # 21 - The first half of a multi statement DML
  --source include/rpl/connection_source.inc
  BEGIN;
  INSERT INTO t1 VALUES (1);
  SET @v=2;
  INSERT INTO t1 VALUES (@v);
  COMMIT;

  # Hang the I/O thread after the User var is received
  --source include/rpl/connection_replica.inc
  --let $rpl_after_received_events_action= hang
  --let $rpl_event_count= 1
  --let $rpl_count_only_event_type= User var
  --let $rpl_continue_event_count_after_hanging= 0
  --source include/rpl/receive_event_count.inc
  # The GTID of the create table must be executed
  --let $gtid_step_count= 0
  --source common/rpl/sync_relay_log_info_assert_pos.inc

  # 22 - Flushed the relay log in the middle of the DML
  FLUSH LOCAL RELAY LOGS;
  --let $gtid_step_count= 0
  --source common/rpl/sync_relay_log_info_assert_pos.inc

  # Assert that the multi statement DML was not partially committed
  --let $assert_text= ($_rrli_subheader) Multi statement DML was not partially committed
  --let $assert_cond= [ SELECT COUNT(*) = 0 FROM t1 ]
  --source include/assert.inc

  # Make SQL thread to fail
  INSERT INTO t1 VALUES (2);
  # Assert that we executed the above transaction
  --let $gtid_step_count= 1
  --let $gtid_step_uuid= `SELECT @@GLOBAL.server_uuid`
  --source include/rpl/gtid_step_assert.inc

  # 23 - The multi statement DML should be rolled back
  SET DEBUG_SYNC= 'now SIGNAL continue_after_queue_event';
  --source include/rpl/connection_source.inc
  --source include/rpl/sync_to_replica_received.inc

  # Wait for the SQL thread to stop with error
  --let $slave_sql_errno= convert_error(ER_DUP_ENTRY)
  --source include/rpl/wait_for_applier_error.inc

  --let $gtid_step_count= 0
  --source common/rpl/sync_relay_log_info_assert_pos.inc

  # Make SQL thread to succeed when retrying the multi statement DML
  DELETE FROM t1 WHERE c1 = 2;
  # Assert that we executed the above transaction
  --let $gtid_step_count= 1
  --let $gtid_step_uuid= `SELECT @@GLOBAL.server_uuid`
  --source include/rpl/gtid_step_assert.inc

  # 24 - The multi statement DML should be rolled back
  --source include/rpl/start_applier.inc
  --source include/rpl/connection_source.inc
  --source include/rpl/sync_to_replica_received.inc
  --let $gtid_step_count= 1
  --let $gtid_step_uuid= $master_uuid
  --source common/rpl/sync_relay_log_info_assert_pos.inc

  # 25 - Simple DDL
  --source include/rpl/connection_source.inc
  DROP TABLE t1;
  --source include/rpl/sync_to_replica_received.inc
  --let $gtid_step_count= 1
  --source common/rpl/sync_relay_log_info_assert_pos.inc
}
