# ==== Purpose ====
#
# Set XCom configuration on all servers.
#
# --let $group_replication_group_name= UUID
# --source include/gr_configuration.inc

--let $include_filename= gr_configuration.inc
--source include/begin_include_file.inc

if ($group_replication_group_name == "")
{
  --die ERROR IN TEST: the "group_replication_group_name" variable must be set
}

# Save current connection.
--let $_old_connection= $CURRENT_CONNECTION


# Default values
--let $_group_replication_group_seeds=
--let $_rpl_gcs_bootstrap_group=
--let $_rpl_gcs_server_address= localhost


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


# Compute group members.
# Order is from last to first server like on rpl_init.inc.
--let $_rpl_server= $_rpl_gcs_server_number
while ($_rpl_server)
{
  --let $_rpl_gcs_server_port= \$SERVER_GR_PORT_$_rpl_server

  # Append comma to list of addresses.
  if ($_group_replication_group_seeds != "")
  {
    --let $_group_replication_group_seeds= $_group_replication_group_seeds,
  }

  --let $_rpl_gcs_member= $_rpl_gcs_server_address:$_rpl_gcs_server_port
  --let $_group_replication_group_seeds= $_group_replication_group_seeds$_rpl_gcs_member

  --dec $_rpl_server
}


# Set XCom configuration on all servers.
# Order is from last to first server like on rpl_init.inc.
--let $_rpl_server= $_rpl_gcs_server_number
while ($_rpl_server)
{
  --let $_rpl_connection_name= server$_rpl_server
  --connection $_rpl_connection_name

  # Boot group on last server (server with bigger server_id).
  --let $_group_replication_bootstrap_group= 0
  if ($_rpl_gcs_bootstrap_group == "")
  {
    --let $_rpl_gcs_bootstrap_group= "1"
    --let $_group_replication_bootstrap_group= 1
  }

  # Local address.
  --let $_rpl_gcs_server_port= \$SERVER_GR_PORT_$_rpl_server
  --let $_group_replication_local_address= $_rpl_gcs_server_address:$_rpl_gcs_server_port

  # Set configuration on server.
  --disable_query_log
  --eval SET GLOBAL group_replication_local_address= "$_group_replication_local_address"
  --eval SET GLOBAL group_replication_group_seeds= "$_group_replication_group_seeds"
  --eval SET GLOBAL group_replication_bootstrap_group= $_group_replication_bootstrap_group

  if (!$skip_recovery_configuration)
  {
    # Set distributed recovery user.
    --disable_warnings
    CHANGE MASTER TO MASTER_USER="root" FOR CHANNEL "group_replication_recovery";
    --enable_warnings
  }

  --error 0, ER_SLAVE_CHANNEL_DOES_NOT_EXIST
  RESET SLAVE ALL FOR CHANNEL "group_replication_applier";

  --enable_query_log

  --dec $_rpl_server
}


# Revert to old connection.
--connection $_old_connection

--let $include_filename= gr_configuration.inc
--source include/end_include_file.inc
