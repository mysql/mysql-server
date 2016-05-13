# ==== Purpose ====
#
# After a server has restarted, this waits for all clients configured
# by rpl_init.inc and/or master-slave.inc to reconnect again.
#
# For each connection, it issues this:
#   --enable_reconnect
#   --source include/wait_until_connected_again.inc
#   --disable_reconnect
#
#
# ==== Usage ====
#
# --let $rpl_server_number= N
# [--let $rpl_debug= 1]
# --source include/rpl_reconnect.inc
#
# Parameters:
#   $rpl_server_number
#     Number to identify the server that needs to reconnect.  1 is the
#     master server, 2 the slave server, 3 the 3rd server, and so on.
#     Cf. include/rpl_init.inc
#
#   $rpl_debug
#     See include/rpl_init.inc

--let $include_filename= rpl_reconnect.inc
--source include/begin_include_file.inc

if (!$rpl_server_number)
{
  --die ERROR IN TEST: you must set $rpl_server_number before you source rpl_connect.inc
}


if ($rpl_debug)
{
  --echo ---- Enable reconnect ----
}

--let $_rpl_server_number= $rpl_server_number

--dec $_rpl_server_number
if (!$_rpl_server_number)
{
  --let $rpl_connection_name= default
  --source include/rpl_connection.inc
  --enable_reconnect

  --let $rpl_connection_name= master
  --source include/rpl_connection.inc
  --enable_reconnect

  --let $rpl_connection_name= master1
  --source include/rpl_connection.inc
  --enable_reconnect
}

--dec $_rpl_server_number
if (!$_rpl_server_number)
{
  --let $rpl_connection_name= slave
  --source include/rpl_connection.inc
  --enable_reconnect

  --let $rpl_connection_name= slave1
  --source include/rpl_connection.inc
  --enable_reconnect
}

--let $rpl_connection_name= server_$rpl_server_number
--source include/rpl_connection.inc
--enable_reconnect

--let $_rpl_one= _1
--let $rpl_connection_name= server_$rpl_server_number$_rpl_one
--source include/rpl_connection.inc
--enable_reconnect

if ($rpl_debug)
{
  --echo ---- Wait for reconnect and disable reconnect on all connections ----
}

--let $_rpl_server_number= $rpl_server_number

--dec $_rpl_server_number
if (!$_rpl_server_number)
{
  --let $rpl_connection_name= default
  --source include/rpl_connection.inc
  --source include/wait_until_connected_again.inc
  --disable_reconnect

  --let $rpl_connection_name= master
  --source include/rpl_connection.inc
  --source include/wait_until_connected_again.inc
  --disable_reconnect

  --let $rpl_connection_name= master1
  --source include/rpl_connection.inc
  --source include/wait_until_connected_again.inc
  --disable_reconnect
}

--dec $_rpl_server_number
if (!$_rpl_server_number)
{
  --let $rpl_connection_name= slave
  --source include/rpl_connection.inc
  --source include/wait_until_connected_again.inc
  --disable_reconnect

  --let $rpl_connection_name= slave1
  --source include/rpl_connection.inc
  --source include/wait_until_connected_again.inc
  --disable_reconnect
}

--let $rpl_connection_name= server_$rpl_server_number
--source include/rpl_connection.inc
--source include/wait_until_connected_again.inc
--disable_reconnect

--let $rpl_connection_name= server_$rpl_server_number$_rpl_one
--source include/rpl_connection.inc
--source include/wait_until_connected_again.inc
--disable_reconnect


--let $include_filename= rpl_reconnect.inc
--source include/end_include_file.inc
