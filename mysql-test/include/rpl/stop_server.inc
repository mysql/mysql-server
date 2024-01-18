# ==== Purpose ====
#
# Stop the server given by $rpl_server_number.
#
# ==== Usage ====
#
# --let $rpl_server_number= N
# [--let $rpl_debug= 1]
# [--let $rpl_force_stop= 1]
# --source include/rpl/stop_server.inc
#
# Parameters:
#
#   $rpl_server_number
#     Number to identify the server that needs to reconnect.  1 is the
#     master server, 2 the slave server, 3 the 3rd server, and so on.
#     Cf. include/rpl/init.inc
#
#   $rpl_debug
#     See include/rpl/init.inc
#   $rpl_force_stop
#     Kill the server immediately i.e., --shutdown_server 0
#
# ==== See also ====
#
# rpl/start_server.inc
# rpl/restart_server.inc


# Can't use begin_include_file / end_include_file because they require
# executing on a server and the server will go down after this script.
if (!$_include_file_depth)
{
  --echo include/rpl/stop_server.inc [server_number=$rpl_server_number]
}
--inc $_include_file_depth
--let $_rpl_stop_server_old_connection= $CURRENT_CONNECTION
if ($rpl_debug)
{
  --echo $_include_file_indent con='$CURRENT_CONNECTION' warn='$ENABLE_WARNINGS' qlog='$ENABLE_QUERY_LOG' rlog='$ENABLE_RESULT_LOG' aborterr='$ENABLE_ABORT_ON_ERROR'
  --echo $_include_file_indent==== BEGIN include/rpl/stop_server.inc ====
}


--let $rpl_connection_name= server_$rpl_server_number
--source include/connection.inc

# Write file to make mysql-test-run.pl expect the "crash", but don't start
# it until it's told to
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.$rpl_server_number.expect

# Send shutdown to the connected server.

--let $ss_timeout= 60

if ($shutdown_server_timeout)
{
  --let $ss_timeout= $shutdown_server_timeout
}

--let $timeout_multiplier= 6

if ($VALGRIND_TEST)
{
  --expr $ss_timeout= $ss_timeout * $timeout_multiplier
}

let $have_sparc = `select convert(@@version_compile_machine using latin1) IN ('sparc')`;
if ($have_sparc)
{
  --expr $ss_timeout= $ss_timeout * $timeout_multiplier
}

if ($rpl_force_stop)
{
   # if force_stop is set, shutdown server immediately(equal to killing the server)
   --let $ss_timeout= 0
}

shutdown_server $ss_timeout;

--source include/wait_until_disconnected.inc

--let $rpl_connection_name= $_rpl_stop_server_old_connection
--source include/connection.inc
--dec $_include_file_depth
if ($rpl_debug)
{
  --echo $_include_file_indent==== END include/rpl/stop_server.inc [server_number=$rpl_server_number] ====
  --echo $_include_file_indent con='$CURRENT_CONNECTION' warn='$ENABLE_WARNINGS' qlog='$ENABLE_QUERY_LOG' rlog='$ENABLE_RESULT_LOG' aborterr='$ENABLE_ABORT_ON_ERROR'
}
