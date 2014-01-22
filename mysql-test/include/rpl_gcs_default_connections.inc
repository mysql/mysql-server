# ==== Purpose ====
#
# Create the default connections to the cluster nodes server1,
# server2, server3...
# If you need more than 2 servers, please add them to cnf file.
# Example:
##!include ../my.cnf
##
##[mysqld.3]
##
##[mysqld.4]
##
##[ENV]
##SERVER_MYPORT_3=    @mysqld.3.port
##SERVER_MYSOCK_3=    @mysqld.3.socket
##
##SERVER_MYPORT_4=    @mysqld.4.port
##SERVER_MYSOCK_4=    @mysqld.4.socket
#
# If master.opt file exists, it will be applied to servers 3 and
# greater.
#
# ==== Usage ====
#
# --source include/rpl_gcs_default_connections.inc

--let $include_filename= rpl_gcs_default_connections.inc
--source include/begin_include_file.inc

--let $_rpl_server_max= 10
--let $_rpl_server_number= 1
while ($_rpl_server_number <= $_rpl_server_max)
{
  --let $_rpl_server_port= \$SERVER_MYPORT_$_rpl_server_number
  if (!$_rpl_server_port)
  {
    # We exceeded the number of servers specified on cnf file.
    --let $_rpl_server_number= $_rpl_server_max
  }

  if ($_rpl_server_port)
  {
    --let $rpl_connection_name= server$_rpl_server_number
    --let $rpl_server_number= $_rpl_server_number
    --source include/rpl_connect.inc
  }

  --inc $_rpl_server_number
}

--let $include_filename= rpl_gcs_default_connections.inc
--source include/end_include_file.inc
