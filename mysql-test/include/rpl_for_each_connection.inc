# ==== Purpose ====
#
# Execute a .inc file once for master-slave connection that was
# configured by rpl_init.inc.  For each connection, it sets the
# following variables:
#
# $rpl_master, $rpl_slave
#   The server number of the master and of the slave.
#
# $rpl_channel_name
#   If $rpl_multi_source is set, the name of the channel by which
#   the slave connects to the master. Otherwise the empty string.
#
# $rpl_connection_number
#   1 for the first connection, 2 for the second connection, etc.
#
# Moreover, the current connection will be set to server_$rpl_slave.
#
# ==== Usage ====
#
# --let $rpl_source_file= FILE
# [--let $rpl_multi_source= 1]
# [--let $rpl_debug= 1]
# --source include/rpl_for_each_connection.inc
#
# Parameters:
#   $rpl_source_file
#     The file that will be sourced.
#
#   $rpl_multi_source
#     If this is set, $rpl_channel_name is set.
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= rpl_for_each_connection.inc [$rpl_source_file]
--source include/begin_include_file.inc

--let $rpl_connection_number= 1
while ($rpl_connection_number <= $rpl_connection_count)
{
  # 'mX sY mZ sW ...'
  #        ^ _rfec_offset points here in the second iteration
  --let $_rfec_offset= 1 + ($rpl_connection_number - 1) * 2 * (2 + $rpl_server_count_length)
  --let $rpl_master= `SELECT SUBSTRING('$rpl_connection_list', $_rfec_offset + 1, $rpl_server_count_length)`
  --let $rpl_slave= `SELECT SUBSTRING('$rpl_connection_list', $_rfec_offset + 1 + $rpl_server_count_length + 2, $rpl_server_count_length)`

  --let $rpl_channel_name=
  if ($rpl_multi_source)
  {
    --let $rpl_channel_name= channel_$rpl_master
  }

  if ($rpl_debug)
  {
    --echo # debug: rpl_for_each_connection.inc: connection=$rpl_connection master=$rpl_master slave=$rpl_slave
  }

  --let $rpl_connection_name= server_$rpl_slave
  --source include/rpl_connection.inc
  --source $rpl_source_file

  --inc $rpl_connection_number
}

--let $rpl_master=
--let $rpl_slave=
--let $rpl_connection_number=
--let $rpl_channel_name=

--let $include_filename= rpl_for_each_connection.inc [$rpl_source_file]
--source include/end_include_file.inc
