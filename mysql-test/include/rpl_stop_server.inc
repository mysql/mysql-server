# ==== Purpose ====
#
# Stop the server given by $rpl_server_number.
#
# ==== Usage ====
#
# --let $rpl_server_number= N
# [--let $rpl_debug= 1]
# --source include/rpl_stop_server.inc
#
# Parameters:
#
#   $rpl_server_number
#     Number to identify the server that needs to reconnect.  1 is the
#     master server, 2 the slave server, 3 the 3rd server, and so on.
#     Cf. include/rpl_init.inc
#
#   $rpl_debug
#     See include/rpl_init.inc
#
# ==== See also ====
#
# rpl_start_server.inc
# rpl_restart_server.inc


# Can't use begin_include_file / end_include_file because they require
# executing on a server and the server will go down after this script.
if (!$_include_file_depth)
{
  --echo include/rpl_stop_server.inc [server_number=$rpl_server_number]
}
--inc $_include_file_depth
--let $_rpl_stop_server_old_connection= $CURRENT_CONNECTION
if ($rpl_debug)
{
  --echo $_include_file_indent con='$CURRENT_CONNECTION' warn='$ENABLED_WARNINGS' qlog='$ENABLED_QUERY_LOG' rlog='$ENABLED_RESULT_LOG' aborterr='$ENABLED_ABORT_ON_ERROR'
  --echo $_include_file_indent==== BEGIN include/rpl_stop_server.inc ====
}


--let $rpl_connection_name= server_$rpl_server_number
--source include/rpl_connection.inc

# Write file to make mysql-test-run.pl expect the "crash", but don't start
# it until it's told to
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.$rpl_server_number.expect

# Send shutdown to the connected server and give
# it 14 seconds to die before zapping it
shutdown_server 14;

--source include/wait_until_disconnected.inc


--let $rpl_connection_name= $_rpl_stop_server_old_connection
--source include/rpl_connection.inc
--dec $_include_file_depth
if ($rpl_debug)
{
  --echo $_include_file_indent==== END include/rpl_stop_server.inc [server_number=$rpl_server_number] ====
  --echo $_include_file_indent con='$CURRENT_CONNECTION' warn='$ENABLED_WARNINGS' qlog='$ENABLED_QUERY_LOG' rlog='$ENABLED_RESULT_LOG' aborterr='$ENABLED_ABORT_ON_ERROR'
}
