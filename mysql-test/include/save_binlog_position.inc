# ==== Purpose ====
#
# Save current filename and offset for the binary log in the mysqltest
# variables $binlog_file and $binlog_position. The positions can be
# used in any way you like. The variable names are deliberately chosen
# so that this script can be used together with
# include/assert_binlog_events.inc

--let $include_filename= save_binlog_position.inc
--source include/begin_include_file.inc

--let $binlog_file= query_get_value(SHOW MASTER STATUS, File, 1)
--let $binlog_position= query_get_value(SHOW MASTER STATUS, Position, 1)
if ($rpl_debug)
{
  --echo saved binlog_file='$binlog_file' binlog_position='$binlog_position'
}

--let $include_filename= save_binlog_position.inc
--source include/end_include_file.inc
