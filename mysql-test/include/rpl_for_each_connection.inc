# ==== Purpose ====
#
# Execute a .inc file once for master-slave connection that was
# configured by rpl_init.inc.  For each connection, $rpl_master and
# $rpl_slave will be set to the number of the master and of the slave,
# and the currenct connection will be set to server_N, where
# N == $rpl_slave. Moreover, $rpl_connection_name will be set to 1
# for the first connection, 2 for the second connection, etc.
#
# ==== Usage ====
#
# my_file.inc:
# --echo $rpl_master is a master of $rpl_slave
# EOF
#
# --let $rpl_source_file= FILE
# [--let $rpl_debug= 1]
# --source include/rpl_for_each_connection.inc
#
# Parameters:
#   $rpl_source_file
#     The file that will be sourced.
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= rpl_for_each_connection.inc [$rpl_source_file]
--source include/begin_include_file.inc

--let $rpl_connection_number= 1
while ($rpl_connection_number <= $rpl_connection_count)
{
#--echo RPL_FOR_EACH_CONNECTION
  # 'mX sY mZ sW ...'
  #        ^ _rfec_offset points here in the second iteration
  --let $_rfec_offset= 1 + ($rpl_connection_number - 1) * 2 * (2 + $rpl_server_count_length)
  --let $rpl_master= `SELECT SUBSTRING('$rpl_connection_list', $_rfec_offset + 1, $rpl_server_count_length)`
  --let $rpl_slave= `SELECT SUBSTRING('$rpl_connection_list', $_rfec_offset + 1 + $rpl_server_count_length + 2, $rpl_server_count_length)`

  if ($rpl_debug)
  {
    --echo # debug: rpl_for_each_connection.inc: connection=$rpl_connection master=$rpl_master slave=$rpl_slave
  }

  --let $rpl_connection_name= server_$rpl_slave
  --source include/rpl_connection.inc
  --source $rpl_source_file

  --inc $rpl_connection_number
}

--let $include_filename= rpl_for_each_connection.inc [$rpl_source_file]
--source include/end_include_file.inc
