##################################################
# Author: Jeb
# Date:   2007/04
# Purpose: To select out log name and start
#          positions from ndb_apply_status table
##################################################
echo select log_name, start_pos from mysql.ndb_apply_status;

if (!$source_file) {
  die Missing value for $source_file;
}
if (!$source_start_pos) {
  die Missing value for $source_start_pos;
}
if (!$source_end_pos) {
  die Missing value for $source_end_pos;
}

let $binlog_file= `SELECT log_name FROM mysql.ndb_apply_status
                    WHERE server_id = $source_server_id`;
#echo binlog_file: $binlog_file;

let $binlog_start = `SELECT start_pos FROM mysql.ndb_apply_status
                    WHERE server_id = $source_server_id`;
#echo binlog_start: $binlog_start;

let $binlog_end = `SELECT end_pos FROM mysql.ndb_apply_status
                    WHERE server_id = $source_server_id`;
#echo binlog_end: $binlog_end;

if ($binlog_file != $source_file) {
  echo Detected mismatching file;
  echo source_server_id: $source_server_id;
  echo source_file: $source_file;
  echo binlog_file: $binlog_file;
  eval SELECT log_name, start_pos, end_pos
    FROM mysql.ndb_apply_status WHERE server_id = $source_server_id;
  SELECT * FROM mysql.ndb_apply_status;
  --source include/rpl/connection_source.inc
  show binlog events;
  die Mismatching log_name in ndb_apply_status;
}

if ($binlog_start != $source_start_pos) {
  echo Detected mismatching start_pos;
  echo source_server_id: $source_server_id;
  echo source_start_pos: $source_start_pos;
  echo binlog_start: $binlog_start;
  eval SELECT log_name, start_pos, end_pos
    FROM mysql.ndb_apply_status WHERE server_id = $source_server_id;
  SELECT * FROM mysql.ndb_apply_status;
  --source include/rpl/connection_source.inc
  show binlog events;
  die Mismatching start_pos in ndb_apply_status;
}

if ($binlog_end != $source_end_pos) {
  echo Detected mismatching end_pos;
  echo source_server_id: $source_server_id;
  echo source_end_pos: $source_end_pos;
  echo binlog_end: $binlog_end;
  eval SELECT log_name, start_pos, end_pos
    FROM mysql.ndb_apply_status WHERE server_id = $source_server_id;
  SELECT * FROM mysql.ndb_apply_status;
  --source include/rpl/connection_source.inc
  show binlog events;
  die Mismatching end_pos in ndb_apply_status;
}

# Reset parameters to make sure they are assigned by caller
let $source_file =;
let $source_start_pos =;
let $source_end_pos =;
