#echo MYSQLD1_PORT $MYSQLD1_PORT;
#echo MYSQLD1_SOCK $MYSQLD1_SOCK;

#query_vertical SHOW MASTER STATUS;
#SHOW REPLICAS; # list of replicas currently registered with the source

#echo MYSQLD2_PORT $MYSQLD2_PORT;
#echo MYSQLD2_SOCK $MYSQLD2_SOCK;

--echo # Save source binlog position
let $source_file= query_get_value(SHOW MASTER STATUS, File, 1);
let $source_pos= query_get_value(SHOW MASTER STATUS, Position, 1);
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

START REPLICA SQL_THREAD;

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

connection default;

