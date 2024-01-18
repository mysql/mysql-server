# ==== Purpose ====
#
# Clear group_replication_bootstrap_group option on all servers.
#
# --source include/gr_clear_bootstrap_group.inc

--let $_include_silent_group_replication_reset_bootstrap_group= $include_silent
--let $include_silent= 1
--let $include_filename= gr_clear_bootstrap_group.inc
--source include/begin_include_file.inc

# Save current connection.
--let $_old_connection= $CURRENT_CONNECTION


# Compute number of servers.
--let $_rpl_server_max= 10
--let $_rpl_server= 1
while ($_rpl_server <= $_rpl_server_max)
{
  --let $_rpl_server_port= \$SERVER_MYPORT_$_rpl_server
  if (!$_rpl_server_port)
  {
    # We exceeded the number of servers specified on cnf file.
    # Save number of configured servers.
    --let $_rpl_gcs_server_number= $_rpl_server
    --dec $_rpl_gcs_server_number

    # Break loop.
    --let $_rpl_server= $_rpl_server_max
  }

  --inc $_rpl_server
}


# Set XCom configuration on all servers.
# Order is from last to first server like on rpl/init.inc.
--let $_rpl_server= $_rpl_gcs_server_number
while ($_rpl_server)
{
  --let $_rpl_connection_name= server$_rpl_server
  --connection $_rpl_connection_name

  # Clear group_replication_bootstrap_group option.
  --disable_query_log
  --eval SET GLOBAL group_replication_bootstrap_group= 0
  --enable_query_log

  --dec $_rpl_server
}


# Revert to old connection.
--connection $_old_connection

--let $include_filename= gr_clear_bootstrap_group.inc
--source include/end_include_file.inc

--let $include_silent= $_include_silent_group_replication_reset_bootstrap_group
