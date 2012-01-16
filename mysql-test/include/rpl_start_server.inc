# ==== Purpose ====
#
# Start the server given by $rpl_server_number.  This should normally
# be invoked after rpl_stop_server.inc.
#
# ==== Usage ====
#
# --let $rpl_server_number= N
# [--let $rpl_start_with_gtids= 1]
# [--let $rpl_server_parameters= --flag1 --flag2 ...]
# [--let $rpl_debug= 1]
# --source include/rpl_start_server.inc
#
# Parameters:
#
#   $rpl_server_number
#     Number to identify the server that needs to reconnect.  1 is the
#     master server, 2 the slave server, 3 the 3rd server, and so on.
#     Cf. include/rpl_init.inc
#
#   $rpl_start_with_gtids
#     If set, the server will start with GTIDs on, i.e.:
#       --gtid-mode=on --disable-gtid-unsafe-statements
#       --log-bin --log-slave-updates
#       --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB
#
#   $rpl_server_parameters
#     If set, extra parameters given by this variable are passed to
#     mysqld.
#
#   $rpl_debug
#     See include/rpl_init.inc
#
# ==== See also ====
#
# rpl_stop_server.inc
# rpl_restart_server.inc

# Pretty-printed text
--let $_rpl_start_server_info= server_number=$rpl_server_number
# Actual arguments sent to mysqld
--let $_rpl_start_server_parameters= $rpl_server_parameters
# Command printed to mysqld.N.expect file
--let $_rpl_start_server_command= restart

if ($rpl_start_with_gtids)
{
  --let $_rpl_start_server_info= $_rpl_start_server_info gtids=on
  --let $_rpl_start_server_parameters= $_rpl_start_server_parameters --log-bin --log-slave-updates --disable-gtid-unsafe-statements --gtid-mode=on --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB
}
if ($rpl_server_parameters != '')
{
  --let $_rpl_start_server_info= $_rpl_start_server_info parameters: $rpl_server_parameters
}
if ($_rpl_start_server_parameters != '')
{
  --let $_rpl_start_server_command= restart:$_rpl_start_server_parameters
}
if ($rpl_debug)
{
  --echo # debug: rpl_server_number='$rpl_server_number' rpl_start_with_gtids='$rpl_start_with_gtids' rpl_server_parameters='$rpl_server_parameters' _rpl_start_server_parameters='$_rpl_start_server_parameters'
}

--let $include_filename= rpl_start_server.inc [$_rpl_start_server_info]
--source include/begin_include_file.inc

--let $rpl_connection_name= server_$rpl_server_number
--source include/rpl_connection.inc

# Write file to make mysql-test-run.pl start up the server again
--exec echo "$_rpl_start_server_command" > $MYSQLTEST_VARDIR/tmp/mysqld.$rpl_server_number.expect

--source include/rpl_reconnect.inc


--let $include_filename= rpl_start_server.inc [$_rpl_start_server_info]
--source include/end_include_file.inc
