# replicate_mysqld2.inc - Replicate binlog from mysqld1 to mysqld2

--echo # Save source binlog position
let $source_file= query_get_value(SHOW BINARY LOG STATUS, File, 1);
let $source_pos= query_get_value(SHOW BINARY LOG STATUS, Position, 1);
#echo source_file: $source_file;
#echo source_pos: $source_pos;

--echo ## Connect to second mysqld, then start replicating from first mysqld
--connect(mysqld2,127.0.0.1,root,,test,$MYSQLD2_PORT,)


# Verify sbtest database was cleaned up by sysbench
let $db_count= `SELECT COUNT(*) FROM information_schema.SCHEMATA WHERE SCHEMA_NAME = 'sbtest'`;
if ($db_count != 0) {
  die sbtest database should not exist at this point;
}

--replace_result $MYSQLD1_PORT MYSQLD1_PORT
eval CHANGE REPLICATION SOURCE TO SOURCE_HOST='127.0.0.1', SOURCE_PORT=$MYSQLD1_PORT;

START REPLICA IO_THREAD USER = 'root' PASSWORD = '';

--echo #
--echo # Wait for IO thread to fetch all binlog
--echo #
let $wait = 1;
while ($wait) {
  sleep 0.1;
  let $io_file= query_get_value(SHOW REPLICA STATUS, Source_Log_File, 1);
  let $io_pos= query_get_value(SHOW REPLICA STATUS, Read_Source_Log_Pos, 1);
  
  if ($io_file == $source_file) {
    if ($io_pos == $source_pos) {
      --echo The IO thread has fetched all binlog!
      let $wait = 0;
    }
  }
}

--disable_query_log
CREATE TEMPORARY TABLE counters (name varchar(100));
INSERT INTO counters VALUES
  ('Ndb_api_bytes_sent_count_replica'), /* number of bytes sent */
  ('Ndb_api_bytes_received_count_replica'), /* number of bytes received */
  ('Ndb_api_pk_op_count_replica'), /* primary key operations */
  ('Ndb_api_trans_commit_count_replica'), /* commits in NDB */
  ('Ndb_api_wait_exec_complete_count_replica'), /* roundtrips to NDB */
  ('Ndb_api_wait_nanos_count_replica'), /* wait for NDB */
  ('Ndb_replica_trans_apply_nanos'), /* time applying */
  ('Ndb_replica_trans_gap_nanos'); /* wait for server */
CREATE TEMPORARY TABLE stats (name varchar(64) PRIMARY KEY, start BIGINT, end BIGINT);
--enable_query_log

# Init stats with 0
--disable_query_log
INSERT INTO stats SELECT name, 0, 0 FROM counters;
--enable_query_log

let $start_time = `SELECT NOW(6)`;

--echo # Start SQL Thread
START REPLICA SQL_THREAD;

--echo #
--echo # Wait for SQL thread to apply all binlog
--echo #
let $wait = 1;
while ($wait) {
  sleep 0.1;
  let $exec_pos= query_get_value(SHOW REPLICA STATUS, Exec_Source_Log_Pos, 1);
  
  if ($exec_pos == $source_pos) {
      --echo The SQL thread has applied all binlog!
      let $wait = 0;
  }
}

let $end_time = `SELECT NOW(6)`;

--echo # Stop Replica after work is done
STOP REPLICA;

# Collect end counters
--disable_query_log ONCE
UPDATE stats, (SELECT * FROM performance_schema.global_status WHERE VARIABLE_NAME IN (SELECT name from counters)) AS b SET end=b.VARIABLE_VALUE WHERE name=b.VARIABLE_NAME;

# Fetch Replica Settings
let $r_workers= `SELECT VARIABLE_VALUE FROM performance_schema.global_variables WHERE VARIABLE_NAME = 'replica_parallel_workers'`;
let $r_batching= `SELECT VARIABLE_VALUE FROM performance_schema.global_variables WHERE VARIABLE_NAME = 'replica_allow_batching'`;
let $r_preserve= `SELECT VARIABLE_VALUE FROM performance_schema.global_variables WHERE VARIABLE_NAME = 'replica_preserve_commit_order'`;

# Prepare test info string for report
let $test_info= TEST: Time=$CRUNCH_TIME s, Rows=$CRUNCH_ROWS, Threads=$CRUNCH_THREADS;
let $config_info= REPLICA: Workers=$r_workers, Batching=$r_batching, Preserve=$r_preserve;

# Output to var/log/report_crunch_update.log
--source suite/ndbcrunch/report_stats.inc

RESET REPLICA ALL;
disconnect mysqld2;

# Wait for the Binlog Dump thread to disappear on mysqld1
--connect(mysqld1,127.0.0.1,root,,test,$MYSQLD1_PORT,)
--source include/rpl/stop_dump_threads.inc
disconnect mysqld1;

connection default;
