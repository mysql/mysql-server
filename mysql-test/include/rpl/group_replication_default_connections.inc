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
# [--let $rpl_group_replication_reconnect= 1]
# --source include/rpl/group_replication_default_connections.inc
#
# Parameters:
#   $rpl_group_replication_reconnect
#     If this parameter is set then disconnect current connection.
#     This is to be used when connection is open and we want to
#     reconnect so session variable can be reinitialized with
#     its corresponding global value.

--let $include_filename= rpl/group_replication_default_connections.inc
--source include/begin_include_file.inc

# To distinguish between rpl and group_replication suite, we are
# setting $using_rpl_group_replication_default_connections.
# As only group_replication suite includes it, so this variable
# will be true only for group_replication suite.

--let $using_rpl_group_replication_default_connections= 1
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

    # reconnection
    if ($rpl_group_replication_reconnect)
    {
      --disconnect $rpl_connection_name
    }
    --source include/rpl/connect.inc
  }

  --inc $_rpl_server_number
}

# unset the value so next call doesn't reconnect without
# explicitly setting $rpl_group_replication_reconnect
--let $rpl_group_replication_reconnect= 0

--let $include_filename= rpl/group_replication_default_connections.inc
--source include/end_include_file.inc
