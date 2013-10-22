# ==== Purpose ====
#
# Create the default connections to the cluster nodes server1, server1_1,
# server2 and server2_1.
#
# The node's connections are only created if $node_connection >= 2.
#
# ==== Usage ====
#
# --let $node_connection= N
# --source include/rpl_gcs_default_connections.inc

--let $include_filename= rpl_gcs_default_connections.inc
--source include/begin_include_file.inc

--let $rpl_connection_name= server1
--let $rpl_server_number= 1
--source include/rpl_connect.inc

#--let $rpl_connection_name= server1_1
#--let $rpl_server_number= 1
#--source include/rpl_connect.inc

if ($node_connection >= 2)
{
  --let $rpl_connection_name= server2
  --let $rpl_server_number= 2
  --source include/rpl_connect.inc

#  --let $rpl_connection_name= server2_1
#  --let $rpl_server_number= 2
#  --source include/rpl_connect.inc
}

--let $include_filename= rpl_gcs_default_connections.inc
--source include/end_include_file.inc
