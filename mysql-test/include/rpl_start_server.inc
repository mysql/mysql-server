# ==== Purpose ====
#
# Start the server given by $rpl_server_number.  This should normally
# be invoked after rpl_stop_server.inc.
#
# ==== Usage ====
#
# --let $rpl_server_number= N
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


--let $_rpl_start_server_command= restart
--let $_rpl_start_server_args= [server_number=$rpl_server_number]
if ($rpl_server_parameters)
{
  --let $_rpl_start_server_command= restart:$rpl_server_parameters
  --let $_rpl_start_server_args= [server_number=$rpl_server_number parameters: $rpl_server_parameters]
}

--let $include_filename= rpl_start_server.inc $_rpl_start_server_args
--source include/begin_include_file.inc

--let $rpl_connection_name= server_$rpl_server_number
--source include/rpl_connection.inc

# Write file to make mysql-test-run.pl start up the server again
--exec echo "$_rpl_start_server_command" > $MYSQLTEST_VARDIR/tmp/mysqld.$rpl_server_number.expect

--source include/rpl_reconnect.inc


--let $include_filename= rpl_start_server.inc $_rpl_start_server_args
--source include/end_include_file.inc
