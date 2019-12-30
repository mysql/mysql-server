# ==== Purpose ====
#
# This is an auxiliary file used by rpl_change_topology.inc
#
# The purpose is to change the connection for one server.
#
# ==== Usage ====
#
# --let $rpl_connection_list= <connection list>
# --let $_rpl_old_connection_list= <old connection list>
# [--let $rpl_unconditional_change_master= 1]
# [--let $use_gtids= 1]
# [--let $rpl_master_log_pos= <master log pos list>]
# [--let $rpl_master_log_file= <master log file list>]
# [--let $rpl_privilege_checks_user= 1:'user'@'host',2:NULL]
# [--let $rpl_privilege_checks_user_dont_create_user= 0]
# --let $rpl_source_file= rpl_change_topology_helper.inc
# --source include/rpl_for_each_connection.inc
#
# Parameters:
#   $rpl_connection_list
#     The new topology (see rpl_change_topology.inc)
#
#   $_rpl_old_connection_list
#     The $rpl_connection_list corresponding to the topology before
#     this call.
#
#   other parameters
#     See rpl_change_topology.inc
#
# ==== Implementation ====
#
# This script builds up a CHANGE MASTER query, adding options as follows:
#
# - MASTER_AUTO_POSITION is set to 1 or 0 according to $use_gtids.
# - MASTER_PORT is set according to the SERVER_MYPORT_N variable
# - MASTER_LOG_FILE and MASTER_LOG_POS are set according to
#   $rpl_master_log_file and $rpl_master_log_pos
#
# Finally, it calls CHANGE MASTER with the computed parameters.
#
# If the server was already configured (according to
# $_rpl_old_connection_list), then we skip executing this file.
#
# ==== Note ===
# This file uses $rpl_channel_name in CHANGE MASTER command, which is
# set in rpl_for_each_connection.inc
#

--let $include_filename= rpl_change_topology_helper.inc [rpl_master=$rpl_master rpl_slave=$rpl_slave]
--source include/begin_include_file.inc

# Determine if we really need to execute a CHANGE MASTER.
--let $_rpl_change_topology_helper_skip= 0
if (!$rpl_unconditional_change_master)
{
  if (`SELECT LOCATE(CONCAT('m',
                            RPAD('$rpl_master', $rpl_server_count_length, ' '),
                            ' s',
                            RPAD('$rpl_slave', $rpl_server_count_length, ' '),
                            ' '),
                     '$_rpl_old_connection_list')`)
  {
    --let $_rpl_change_topology_helper_skip= 1
  }
}

if ($rpl_debug)
{
  --echo \$_rpl_server='$_rpl_server' \$_rpl_master='$_rpl_master' \$use_gtids=$use_gtids
}

if (!$_rpl_change_topology_helper_skip)
{
  # Get port number
  --let $_rpl_change_topology_helper_port= \$SERVER_MYPORT_$rpl_master

  # Configure whether we use AUTO_POSITION.
  if ($use_gtids)
  {
    --source include/assert_gtid_mode_on.inc
    --let $_rpl_change_topology_helper_position= MASTER_AUTO_POSITION = 1,
  }
  if (!$use_gtids)
  {
    --let $_rpl_change_topology_helper_position= MASTER_AUTO_POSITION = 0,

    # Get MASTER_LOG_FILE
    --let $_rpl_change_topology_helper_file_index= `SELECT LOCATE('$rpl_slave:', '$rpl_master_log_file')`
    if ($_rpl_change_topology_helper_file_index)
    {
      # Get text from after ':' and before ',', starting at
      # $_rpl_master_log_file
      --let $_rpl_change_topology_helper_master_log_file= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$rpl_master_log_file', $_rpl_change_topology_helper_file_index), ',', 1), ':', -1)`
      --let $_rpl_change_topology_helper_position= MASTER_LOG_FILE = '$_rpl_master_log_file',
    }

    # Get MASTER_LOG_POS
    --let $_rpl_change_topology_helper_pos_index= `SELECT LOCATE('$rpl_slave:', '$rpl_master_log_pos')`
    if ($_rpl_change_topology_helper_pos_index)
    {
      --let $_rpl_change_topology_helper_master_log_pos= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$rpl_master_log_pos', $_rpl_change_topology_helper_pos_index), ',', 1), ':', -1)`
      --let $_rpl_change_topology_helper_position= $_rpl_change_topology_helper_position MASTER_LOG_POS = $_rpl_change_topology_helper_master_log_pos,
    }
  }

  # Get PRIVILEGE_CHECKS_USER for current `$rpl_slave`
  --let $_rpl_change_topology_helper_privilege_checks_index= `SELECT LOCATE('$rpl_slave:', "$rpl_privilege_checks_user")`
  --let $_rpl_change_topology_helper_privilege_checks = NULL
  if ($_rpl_change_topology_helper_privilege_checks_index)
  {
    --let $_rpl_change_topology_helper_privilege_checks= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING("$rpl_privilege_checks_user", $_rpl_change_topology_helper_privilege_checks_index), ',', 1), ':', -1)`
  }
  # Get PRIVILEGE_CHECKS_USER for all slaves
  if (`SELECT $_rpl_change_topology_helper_privilege_checks_index = 0 AND "$_rpl_change_topology_privilege_checks_all" != ""`)
  {
    --let $_rpl_change_topology_helper_privilege_checks= $_rpl_change_topology_privilege_checks_all
  }
  --let $_rpl_change_topology_helper_position= $_rpl_change_topology_helper_position PRIVILEGE_CHECKS_USER = $_rpl_change_topology_helper_privilege_checks,

  # Check if the user should be created if not exists
  --let $_rpl_change_topology_helper_privilege_checks_user_create = 1
  if ($rpl_privilege_checks_user_dont_create_user == 1)
  {
    --let $_rpl_change_topology_helper_privilege_checks_user_create = 0
  }

  # If PRIVILEGE_CHECKS_USER is defined and the user should be created, create
  if (`SELECT "$_rpl_change_topology_helper_privilege_checks" != "NULL" AND $_rpl_change_topology_helper_privilege_checks_user_create = 1`)
  {
    --let $_rpl_change_topology_helper_at_index= `SELECT LOCATE('@', "$_rpl_change_topology_helper_privilege_checks")`
    --let $_rpl_change_topology_helper_username= $_rpl_change_topology_helper_privilege_checks
    if ($_rpl_change_topology_helper_at_index != 0)
    {
        --let $_rpl_change_topology_helper_username= `SELECT SUBSTRING("$_rpl_change_topology_helper_privilege_checks", 1, $_rpl_change_topology_helper_at_index - 1)`
    }
    --let $user_exists = `SELECT EXISTS(SELECT 1 FROM mysql.user WHERE user = $_rpl_change_topology_helper_username)`
    if ($user_exists == 0)
    {
      SET @@sql_log_bin = 0;
      --eval CREATE USER $_rpl_change_topology_helper_privilege_checks
      if ($rpl_skip_grant_privilege_checks_user_roles != 1)
      {
        if ($rpl_privilege_checks_use_roles != '')
        {
          --let $RPL_PRIV_ROLE = 'sql_applier_thread'
          --eval CREATE ROLE $RPL_PRIV_ROLE
          --eval GRANT REPLICATION_APPLIER,SESSION_VARIABLES_ADMIN ON *.* TO $RPL_PRIV_ROLE
          --eval GRANT $RPL_PRIV_ROLE TO $_rpl_change_topology_helper_privilege_checks
          --eval SET DEFAULT ROLE $RPL_PRIV_ROLE TO $_rpl_change_topology_helper_privilege_checks
        }
        if ($rpl_privilege_checks_use_roles == '')
        {
          --eval GRANT REPLICATION_APPLIER,SESSION_VARIABLES_ADMIN ON *.* TO $_rpl_change_topology_helper_privilege_checks
        }
      }
      --let $rpl_change_topology_user_created = $rpl_change_topology_user_created $rpl_slave:$_rpl_change_topology_helper_privilege_checks
      SET @@sql_log_bin = 1;
    }
  }

  --replace_regex /[0-9]{4}/####/
    if (!$rpl_multi_source)
    {
      eval CHANGE MASTER TO MASTER_HOST = '127.0.0.1', MASTER_PORT = $_rpl_change_topology_helper_port, MASTER_USER = 'root', $_rpl_change_topology_helper_position MASTER_CONNECT_RETRY = 1 FOR CHANNEL '';
    }
    if ($rpl_multi_source)
    {
      eval CHANGE MASTER TO MASTER_HOST = '127.0.0.1', MASTER_PORT = $_rpl_change_topology_helper_port, MASTER_USER = 'root', $_rpl_change_topology_helper_position MASTER_CONNECT_RETRY = 1 FOR CHANNEL $rpl_channel_name;
    }

}

if ($_rpl_change_topology_helper_skip)
{
  if (!$use_gtids)
  {
    --let $_master_auto_position= query_get_value(SHOW SLAVE STATUS $rpl_for_channel_clause, Auto_Position, 1)
      if ($_master_auto_position == 1)
      {
        if (!$rpl_multi_source)
        {
          CHANGE MASTER TO MASTER_AUTO_POSITION= 0 FOR CHANNEL '';
        }
        if ($rpl_multi_source)
        {
          eval CHANGE MASTER TO MASTER_AUTO_POSITION=0 FOR CHANNEL $rpl_channel_name;
        }
      }
  }
}


if (!$_rpl_change_topology_helper_skip)
{
  # This un-configures the server so that it's not a slave.
  # After BUG#28796, such configuration is not possible any more.
  #CHANGE MASTER TO MASTER_HOST = '' FOR CHANNEL $rpl_channel_name;
}


--let $include_filename= rpl_change_topology_helper.inc [rpl_master=$rpl_master rpl_slave=$rpl_slave]
--source include/end_include_file.inc
