# ==== Purpose ====
#
# Clear XCom configuration.
#
# ==== Usage ====
#
# --source include/gr_clear_configuration.inc
#

--let $_include_silent_group_replication_reset_configuration= $include_silent
--let $include_silent= 1
--let $include_filename= gr_clear_configuration.inc
--source include/begin_include_file.inc

# Save current connection.
--let $_old_connection= $CURRENT_CONNECTION

# Clean Group Replication XCom options on all servers to avoid false
# errors on MTR check testcase.
--let $_rpl_server_max= 10
--let $_rpl_server= 1
while ($_rpl_server <= $_rpl_server_max)
{
  --let $_rpl_server_port= \$SERVER_MYPORT_$_rpl_server
  if (!$_rpl_server_port)
  {
    # Break loop.
    --let $_rpl_server= $_rpl_server_max
  }

  if ($_rpl_server_port)
  {
    --let $rpl_connection_name= server_aux_$_rpl_server
    --let $rpl_server_number= $_rpl_server
    --source include/rpl_connect.inc
    --let $rpl_connection_name= server_aux_$_rpl_server
    --source include/rpl_connection.inc

    --disable_query_log
    SET GLOBAL group_replication_local_address= "";
    SET GLOBAL group_replication_group_seeds= "";
    SET GLOBAL group_replication_force_members= "";
    SET GLOBAL group_replication_bootstrap_group= OFF;
    SET GLOBAL read_only= FALSE;

    if (!$skip_recovery_configuration)
    {
      # Clean distributed recovery user.
      RESET SLAVE ALL FOR CHANNEL "group_replication_recovery";
    }

    --enable_query_log

    --connection default
    --disconnect server_aux_$_rpl_server
  }

  --inc $_rpl_server
}

# Revert to old connection.
--connection $_old_connection

--let $include_filename= gr_clear_configuration.inc
--source include/end_include_file.inc

--let $include_silent= --let $_include_silent_group_replication_reset_configuration
