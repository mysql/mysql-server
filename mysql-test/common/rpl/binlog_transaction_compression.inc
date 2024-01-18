# saves the current setting of keep_transaction_payload_events
# we will turn it on (even if they are disabled), so we can
# assert that show binlog events shall produce the expected
# output
--let $saved_keep_transaction_payload_events= $keep_transaction_payload_events
--let $keep_transaction_payload_events= 1

--source include/rpl/connection_source.inc
--let $saved_binlog_transaction_compression_master = `SELECT @@global.binlog_transaction_compression`

--source include/rpl/connection_replica.inc
--let $saved_binlog_transaction_compression_slave = `SELECT @@global.binlog_transaction_compression`

# disable compression for the slave applier threads
--source include/rpl/stop_replica.inc
SET @@global.binlog_transaction_compression=FALSE;
--source include/rpl/start_replica.inc

--source include/rpl/connection_source.inc
--source include/rpl/reset.inc

--let $event_sequence_no_compression=!Gtid_or_anon # !Begin # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Xid

--let $event_sequence_compression=!Gtid_or_anon # Transaction_payload # !Begin # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Table_map # Write_rows # Xid

--let $i=1
while($i < 3)
{
  --source include/rpl/connection_source.inc
  --source include/rpl/reset.inc

  --source include/rpl/connection_replica.inc
  --source include/rpl/stop_replica.inc

  if ($i==1)
  {
    --echo # compression DISABLED
    --let $compression_enabled = FALSE
    --let $compression_type = 'NONE'
    --let $event_sequence = $event_sequence_no_compression
  }

  if ($i==2)
  {
    --echo # ZSTD compression ENABLED
    --let $compression_enabled = TRUE
    --let $compression_type = 'ZSTD'
    --let $event_sequence = $event_sequence_compression
  }

  --source include/rpl/connection_source.inc
  --eval SET @@session.binlog_transaction_compression=$compression_enabled

  --source include/rpl/connection_replica.inc
  --source include/rpl/start_receiver.inc
  --source include/rpl/connection_source.inc

  # prepare for the base from where to assert event sequence, i.e., the
  # CREATE TABLE onwards

  CREATE TABLE t1 (c1 INT UNSIGNED PRIMARY KEY, c2 LONGTEXT);
  --let $create_table_pos_master = query_get_value(SHOW BINARY LOG STATUS, Position, 1)

  # now get the position where the CREATE TABLE ended up in the relay log
  --source include/rpl/connection_source.inc
  --source include/rpl/sync_to_replica_received.inc
  --source include/rpl/save_relay_log_file_position.inc
  --let $relay_log_file_to_read=$relay_log_file
  --let $start_reading_relay_log_from=$relay_log_size

  # now get the position where the CREATE TABLE ended up in the slave's binlog
  --source include/rpl/start_applier.inc
  --source include/rpl/connection_source.inc
  --source include/rpl/sync_to_replica.inc
  --let $create_table_pos_slave = query_get_value(SHOW BINARY LOG STATUS, Position, 1)
  --source include/rpl/stop_applier.inc

  # 1. on master insert a large transaction ...
  --source include/rpl/connection_source.inc
  BEGIN;
  --let $nrows = 10
  while ($nrows > 0)
  {
    --eval INSERT INTO t1 VALUES ($nrows, REPEAT('a', 1000000))
    --dec $nrows
  }
  COMMIT;
  --let $checksum_master = query_get_value(CHECKSUM TABLE t1 EXTENDED, Checksum, 1)

  # ... and assert that SHOW BINLOG EVENTS shows the correct output
  --echo BINLOG EVENTS on master [$compression_type]
  --let $binlog_position = $create_table_pos_master
  --let $binlog_file = query_get_value(SHOW BINARY LOG STATUS, File, 1)
  --let $relay_log=0
  --let $include_header_events= 0
  --let $include_trx_payload_events=1
  --source include/rpl/assert_binlog_events.inc

  # 2. on slave, make sure it is copied fine to the relay log ...
  --source include/rpl/connection_source.inc
  --source include/rpl/sync_to_replica_received.inc

  # ... and assert that SHOW RELAYLOG EVENTS shows the correct output
  --echo RELAY LOG EVENTS for $compression_type
  --let $binlog_file = $relay_log_file_to_read
  --let $binlog_position = $start_reading_relay_log_from
  --let $relay_log=1
  --let $include_header_events= 0
  --let $include_trx_payload_events=1
  --source include/rpl/assert_binlog_events.inc
  --let $limit=
  --let $binlog_file=
  --let $event_sequence=
  --let $relay_log=
  --let $include_trx_payload_events=
  --let $include_header_events=
  --let $binlog_position=

  # 3. one slave, make sure it is applied fine
  --source include/rpl/start_applier.inc
  --source include/rpl/connection_source.inc
  --source include/rpl/sync_to_replica.inc

  # 4. lets compare contents of the table on master and slave
  --let $checksum_slave = query_get_value(CHECKSUM TABLE t1 EXTENDED, Checksum, 1)

  # 5. assert that tables have the same contents
  #    We use checksums, because the diff_tables requires
  #    modifications to the sort_buffer_size.
  #    Plain selects cause a valgrind warning in temp tables...
  --let $assert_cond= $checksum_master = $checksum_slave
  --let $assert_text= Assert that master and slave tables have the same content
  --source include/assert.inc

  # 6. on slave, make sure that it logs to its binlog fine...
  # ... and assert that SHOW BINLOG EVENTS shows the correct output
  --echo BINLOG EVENTS on slave [$compression_type] (no compression, since slave applier has binlog_transaction_compression = NONE)
  --let $binlog_position = $create_table_pos_slave
  --let $binlog_file = query_get_value(SHOW BINARY LOG STATUS, File, 1)
  --let $relay_log=0
  --let $include_header_events= 0
  --let $include_trx_payload_events=1
  --let $event_sequence=$event_sequence_no_compression
  --source include/rpl/assert_binlog_events.inc

  # 7. clean up
  --source include/rpl/connection_source.inc
  DROP TABLE t1;
  --source include/rpl/sync_to_replica.inc

  --let $binlog_position =
  --let $binlog_file =
  --let $relay_log=
  --let $include_header_events=
  --let $include_trx_payload_events=
  --let $event_sequence=

  --inc $i
}

--source include/rpl/connection_source.inc

# assert that bytes compressed are a lot less than uncompressed
--source include/rpl/connection_source.inc
--let $compressed_bytes = `SELECT COMPRESSED_BYTES_COUNTER FROM performance_schema.binary_log_transaction_compression_stats WHERE compression_type = 'ZSTD'`
--let $uncompressed_bytes = `SELECT UNCOMPRESSED_BYTES_COUNTER FROM performance_schema.binary_log_transaction_compression_stats WHERE compression_type = 'NONE'`

# assert
--let $assert_cond= ($compressed_bytes * 100) < $uncompressed_bytes
--let $assert_text= Compressed transaction is a lot smaller than uncompressed
--source include/assert.inc

--let $keep_transaction_payload_events= $saved_keep_transaction_payload_events
--let $saved_keep_transaction_payload_events=

--source include/rpl/connection_source.inc
--replace_result $saved_binlog_transaction_compression_master SAVED
--eval SET @@global.binlog_transaction_compression=$saved_binlog_transaction_compression_master

--source include/rpl/connection_replica.inc
--replace_result $saved_binlog_transaction_compression_slave SAVED
--eval SET @@global.binlog_transaction_compression=$saved_binlog_transaction_compression_slave
--source include/rpl/stop_replica.inc
--source include/rpl/start_replica.inc

--source include/rpl/connection_source.inc
--source include/rpl/reset.inc
