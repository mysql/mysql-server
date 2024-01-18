# ==== Purpose ====
#
# Save current filename and offset for the binary log in the mysqltest
# variables $binlog_file and $binlog_position, and the full path to
# the binary log in $binlog_fullpath, and optionally save in the SQL
# variables @binlog_file, @binlog_fullpath, and @binlog_position. The
# positions can be used in any way you like. The variable names are
# deliberately chosen so that this script can be used together with
# include/rpl/assert_binlog_events.inc
#
# ==== Usage ====
#
# Either:
#   --source include/rpl/save_binlog_file_position.inc
#   --echo file=$binlog_file position=$binlog_position fullpath=$binlog_fullpath
#
# or:
#   --let $save_binlog_position_in_sql= 1
#   --source include/rpl/save_binlog_file_position.inc
#   SELECT @binlog_file, @binlog_position, @binlog_fullpath;
#
# Parameters
#   $save_binlog_position_in_sql
#     Save the position in SQL variables on the current connection in
#     addition to the mysqltest variables


--let $include_filename= rpl/save_binlog_file_position.inc
--source include/begin_include_file.inc

--let $binlog_file= query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $binlog_fullpath= `SELECT @@datadir`
--let $binlog_fullpath= $binlog_fullpath/$binlog_file
--let $binlog_position= query_get_value(SHOW BINARY LOG STATUS, Position, 1)
if ($rpl_debug)
{
  --echo saved binlog_file='$binlog_file' binlog_position='$binlog_position' binlog_fullpath='$binlog_fullpath'
}
if ($save_binlog_position_in_sql)
{
  --disable_query_log
  eval SET @binlog_file= '$binlog_file',
           @binlog_fullpath= '$binlog_fullpath',
           @binlog_position= $binlog_position;
}

--let $include_filename= rpl/save_binlog_file_position.inc
--source include/end_include_file.inc
