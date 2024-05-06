#echo MYSQLD1_PORT $MYSQLD1_PORT;
#echo MYSQLD1_SOCK $MYSQLD1_SOCK;

#query_vertical SHOW MASTER STATUS;
#SHOW REPLICAS; # list of replicas currently registered with the source

#echo MYSQLD2_PORT $MYSQLD2_PORT;
#echo MYSQLD2_SOCK $MYSQLD2_SOCK;

--echo # Save source binlog position
let $source_file= query_get_value(SHOW BINARY LOG STATUS, File, 1);
let $source_pos= query_get_value(SHOW BINARY LOG STATUS, Position, 1);
#echo source_file: $source_file;
#echo source_pos: $source_pos;

--echo ## Connect to second mysqld, then start replicating from first mysqld
--connect(mysqld2,127.0.0.1,root,,test,$MYSQLD2_PORT,)

eval
CHANGE REPLICATION SOURCE TO
  SOURCE_HOST='127.0.0.1',
  SOURCE_PORT=$MYSQLD1_PORT;

#query_vertical SHOW REPLICA STATUS;

START REPLICA IO_THREAD
  USER = 'root'
  PASSWORD = '';

--echo #
--echo # Wait for IO thread to fetch all binlog
--echo #
let $wait = 1;
while ($wait) {
  sleep 0.1;
  #query_vertical SHOW REPLICA STATUS;
  let $io_file= query_get_value(SHOW REPLICA STATUS, Source_Log_File, 1);
  let $io_pos= query_get_value(SHOW REPLICA STATUS, Read_Source_Log_Pos, 1);
  #echo io_file: $io_file;
  #echo io_pos: $io_pos;

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

CREATE TEMPORARY TABLE stats (
  name varchar(64) PRIMARY KEY,
  start BIGINT,
  end BIGINT);

# All replica counters are zero before replica has started, thus it's necessary
# to first start the replica and then wait for counters to start updating
#SELECT * FROM performance_schema.global_status
#  WHERE VARIABLE_NAME IN (SELECT name from counters) AND
#        VARIABLE_VALUE != 0;

--enable_query_log

START REPLICA SQL_THREAD;

# Wait for replica to start updating counters
# NOTE! The replica uses "copy out" for its counter values
let $wait = 1;
while ($wait) {
  if (`SELECT VARIABLE_VALUE FROM performance_schema.global_status
         WHERE VARIABLE_NAME = 'Ndb_api_trans_commit_count_replica'`) {
    --echo Detected first replica commit;
    let $wait = 0;

     # Also wait for Ndb_replica_trans_apply_nanos which are slower to
     # start updating (NOTE! usually need several epochs before first
     # measurement shows, should only need two?)
     #if (`SELECT VARIABLE_VALUE FROM performance_schema.global_status
     #        WHERE VARIABLE_NAME = 'Ndb_replica_trans_apply_nanos'`) {
     #  --echo Detected Ndb_replica_trans_apply_nanos measurement;
     #  let $wait = 0;
     #}
  }
  if ($wait) {
    sleep 0.1;
  }
}

# Collect counters
let $start_time = `SELECT NOW(6)`;
--disable_query_log ONCE
INSERT INTO stats
  SELECT VARIABLE_NAME AS name, VARIABLE_VALUE as start, 0 AS end
    FROM performance_schema.global_status
      WHERE VARIABLE_NAME IN (SELECT name from counters);
#SELECT * FROM stats;

--echo #
--echo # Wait for SQL thread to apply binlog
--echo #
let $wait = 1;
while ($wait) {
  sleep 0.1;
  #query_vertical SHOW REPLICA STATUS;
  let $sql_file= query_get_value(SHOW REPLICA STATUS, Relay_Source_Log_File, 1);
  let $sql_pos= query_get_value(SHOW REPLICA STATUS, Exec_Source_Log_Pos, 1);
  #echo sql_file: $sql_file;
  #echo sql_pos: $sql_pos;

  if ($sql_file == $source_file) {
    if ($sql_pos == $source_pos) {
      --echo The SQL thread has fetched all binlog!
      let $wait = 0;
    }
  }
}

STOP REPLICA;

# Collect end counters
let $end_time = `SELECT NOW(6)`;
--disable_query_log ONCE
UPDATE stats,
  (SELECT * FROM performance_schema.global_status
   WHERE VARIABLE_NAME IN (SELECT name from counters)) AS b
   SET end=b.VARIABLE_VALUE WHERE name=b.VARIABLE_NAME;
#SELECT * FROM stats;

if ($CRUNCH_STATS)
{
  # Dump the collected stats
  SELECT * FROM stats;

  # #########################
  # Calculate and print stats

  # Sample period
  let $sample_period = `
    SELECT TIMEDIFF('$end_time', '$start_time') as 'elapsed time'`;
  echo sample_period:          $sample_period;

  # transactions_applied (epochs)
  let $transactions_applied = `SELECT end-start FROM stats
                    WHERE name = 'Ndb_api_trans_commit_count_replica'`;
  echo transactions_applied:   $transactions_applied;

  # bytes
  let $bytes_sent = `SELECT (end-start) / 1024 / 1024 FROM stats
                    WHERE name = 'Ndb_api_bytes_sent_count_replica'`;
  echo bytes_sent (MB):        $bytes_sent;

  # apply_time
  let $apply_time = `
    SELECT (end-start) / 1000000 AS 'transaction apply time (ms)'
      FROM stats WHERE name = 'Ndb_replica_trans_apply_nanos'`;
  echo apply_time(ms):         $apply_time;

  # gap_time
  let $gap_time = `
  SELECT (end-start) / 1000000 AS 'inter-transaction gap (ms)'
    FROM stats WHERE name = 'Ndb_replica_trans_gap_nanos'`;
  echo gap_time(ms):           $gap_time;

  # total_time = apply_time+gap_time (~ Sample period)
#  let $total_time = `SELECT $apply_time + $gap_time`;
#  echo total_time (ms):        $total_time;

  # api_wait_time
  let $api_wait_time = `
    SELECT (end-start) / 1000000 AS 'NdbApi wait time per transaction'
      FROM stats WHERE name = 'Ndb_api_wait_nanos_count_replica'`;
  echo api_wait_time (ms):     $api_wait_time;
  echo;

  #
  # Averages
  #
  #avg_apply_time = apply_time / transactions_applied
#  let $avg_apply_time = `
#    SELECT $apply_time / $transactions_applied`;
#  echo avg_apply_time (ms):    $avg_apply_time;

  #avg_gap_time  = gap_time / transactions_applied
#  let $avg_gap_time = `
#    SELECT $gap_time / $transactions_applied`;
#  echo avg_gap_time (ms):      $avg_gap_time;

  #avg_api_work_time = api_wait_time / transactions_applied
  let $avg_api_work_time = `
    SELECT $api_wait_time / $transactions_applied`;
  echo avg_api_work_time (ms): $avg_api_work_time;
  echo;

  #
  # Percent
  #
  #apply_api_wait_percent = 100 * (api_wait_time / apply_time)
#  let $apply_api_wait_percent = `
#    SELECT 100* ($api_wait_time / $apply_time)`;
#  echo apply_api_wait_percent: $apply_api_wait_percent;

  #apply_work_percent     = 100 - apply_api_wait_percent
#  let $apply_work_percent = `
#    SELECT 100 - $apply_api_wait_percent`;
#  echo apply_work_percent:     $apply_work_percent;

}

connection default;

