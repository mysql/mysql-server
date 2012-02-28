# ==== Purpose ====
#
# Create the default connections master, master1, slave, slave1.
#
# The slave connections are only created if $rpl_server_count >= 2.
#
# ==== Usage ====
#
# --let $rpl_server_count= N
# --source include/rpl_default_connections.inc

--let $include_filename= rpl_default_connections.inc
--source include/begin_include_file.inc

--let $rpl_connection_name= master
--let $rpl_server_number= 1
--source include/rpl_connect.inc

--let $rpl_connection_name= master1
--let $rpl_server_number= 1
--source include/rpl_connect.inc

if ($rpl_server_count >= 2)
{
  --let $rpl_connection_name= slave
  --let $rpl_server_number= 2
  --source include/rpl_connect.inc

  --let $rpl_connection_name= slave1
  --let $rpl_server_number= 2
  --source include/rpl_connect.inc
}

--let $include_filename= rpl_default_connections.inc
--source include/end_include_file.inc
