##################################################
# Author: Jeb
# Date:   2007/04
# Purpose: To select out log name and start
#          positions from ndb_apply_status table
##################################################
echo select log_name, start_pos from mysql.ndb_apply_status;

let $binlog_file= `SELECT log_name FROM mysql.ndb_apply_status
                    WHERE server_id = $master_server_id`;
#echo binlog_file: $binlog_file;

let $binlog_start = `SELECT start_pos FROM mysql.ndb_apply_status
                    WHERE server_id = $master_server_id`;
#echo binlog_start: $binlog_start;


