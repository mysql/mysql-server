# ==== Purpose ====
#
# Create a connection to a given numbered server.
#
# This script is normally used internally by rpl/init.inc and
# rpl/init_source_replica.inc, but it can also be used in test cases that need to
# create more connections or re-create connections after disconnect.
#
#
# ==== Usage ====
#
# --let $rpl_connection_name= <connection_name>
# --let $rpl_server_number= <server_number>
# [--let $rpl_set_gtid_next_anonymous_in_new_connection= 1]
# [--let $rpl_debug= 1]
# --source include/rpl/connect.inc
#
# Parameters:
#   $rpl_connection_name
#     The name of the connection to create.
#
#   $rpl_server_number
#     The number of the server to connect to.
#
#   $rpl_set_gtid_next_anonymous_in_new_connection
#     Executes SET GTID_NEXT = 'ANONYMOUS' in the new connection.
#
#   $rpl_debug
#     See include/rpl/init.inc


--let $include_filename= rpl/connect.inc [creating $rpl_connection_name]
--source include/begin_include_file.inc


if (!$rpl_server_number)
{
  --die ERROR IN TEST: You must set $rpl_server_number before sourcing include/rpl/connect.inc
}
if (!$rpl_connection_name)
{
  --die ERROR IN TEST: You must set $rpl_connection_name before sourcing include/rpl/connect.inc
}

# Get port number
--let $_rpl_port= \$SERVER_MYPORT_$rpl_server_number
if (!$_rpl_port)
{
  --echo Bug in test case: '\$SERVER_MYPORT_$rpl_server_number' not initialized. Check the test's .cfg file.
  --die Not all SERVER_MYPORT_* environment variables are setup correctly.
}

# Create connection.
if ($rpl_debug)
{
  --echo connect ($rpl_connection_name,127.0.0.1,root,,test,$_rpl_port,)
}
--connect ($rpl_connection_name,127.0.0.1,root,,test,$_rpl_port,)

# When debug trace is enabled, this makes it easy to map debug trace
# thread id's to mysqltest connection names, since it prints
# mysqltests' connection name printed to the trace.

# Take a backup since rpl/connection.inc clears $rpl_connection_name.
--let $_rc_rpl_connection_name= $rpl_connection_name

--source include/connection.inc
--let $unused= `SELECT "This is mysqltest's connection '$CURRENT_CONNECTION'"`

if ($rpl_set_gtid_next_anonymous_in_new_connection)
{
  if ($rpl_debug)
  {
    --echo # $_rc_rpl_connection_name> SET @@SESSION.GTID_NEXT = 'ANONYMOUS';
  }
  --disable_query_log
  SET @@SESSION.GTID_NEXT = 'ANONYMOUS';
}

--let $include_filename= rpl/connect.inc
--source include/end_include_file.inc
