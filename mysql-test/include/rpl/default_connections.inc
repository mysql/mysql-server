# ==== Purpose ====
#
# Create the default connections master, master1, slave, slave1.
#
# The slave connections are only created if $rpl_server_count >= 2.
#
# ==== Usage ====
#
# --let $rpl_server_count= N
# [--let $rpl_set_gtid_next_anonymous_in_new_connection= 1]
# --source include/rpl/default_connections.inc
#
# Parameters:
#
#   $rpl_server_count
#     The number of servers configured with rpl/init.inc
#
#   $rpl_set_gtid_next_anonymous_in_new_connection
#     Executes SET GTID_NEXT = 'ANONYMOUS' in each new connection, as well
#     as in the 'default' connection.


--let $include_filename= rpl/default_connections.inc
--source include/begin_include_file.inc

--let $_have_default_connections= 1

--let $rpl_connection_name= master
--let $rpl_server_number= 1
--source include/rpl/connect.inc

--let $rpl_connection_name= master1
--let $rpl_server_number= 1
--source include/rpl/connect.inc

if ($rpl_server_count >= 2)
{
  --let $rpl_connection_name= slave
  --let $rpl_server_number= 2
  --source include/rpl/connect.inc

  --let $rpl_connection_name= slave1
  --let $rpl_server_number= 2
  --source include/rpl/connect.inc
}

if ($rpl_set_gtid_next_anonymous_in_new_connection)
{
  if ($rpl_debug)
  {
    --echo # default> SET @@SESSION.GTID_NEXT = 'ANONYMOUS';
  }
  --disable_query_log
  --connection default
  SET @@SESSION.GTID_NEXT = 'ANONYMOUS';
}

--let $include_filename= rpl/default_connections.inc
--source include/end_include_file.inc
