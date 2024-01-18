# ==== Purpose ====
#
# This is an auxiliary file used by rpl/change_topology.inc
#
# The purpose is to change the connection for one server.
#
# ==== Usage ====
#
# --let $rpl_connection_list= <connection list>
# --let $_rpl_old_connection_list= <old connection list>
# [--let $rpl_unconditional_change_master= 1]
# [--let $use_gtids= 1]
# [--let $rpl_source_log_pos= <source log pos list>]
# [--let $rpl_source_log_file= <source log file list>]
# [--let $rpl_privilege_checks_user= 1:'user'@'host',2:NULL]
# [--let $rpl_privilege_checks_user_dont_create_user= 0]
# [--let $rpl_privilege_checks_user_additional_grants= CREATE,INSERT,SELECT]
# [--let $rpl_require_row_format= *:0,1:0,2:1]
# [--let $rpl_gtid_only= *:0,1:0,2:1]
# --let $rpl_source_file= rpl/impl/change_topology_helper.inc
# --source include/rpl/for_each_connection.inc
#
# Parameters:
#   $rpl_connection_list
#     The new topology (see rpl/change_topology.inc)
#
#   $_rpl_old_connection_list
#     The $rpl_connection_list corresponding to the topology before
#     this call.
#
#   other parameters
#     See rpl/change_topology.inc
#
# ==== Implementation ====
#
# This script builds up a CHANGE REPLICATION SOURCE query, adding
# options as follows:
#
# - SOURCE_AUTO_POSITION is set to 1 or 0 according to $use_gtids.
# - SOURCE_PORT is set according to the SERVER_MYPORT_N variable
# - SOURCE_LOG_FILE and SOURCE_LOG_POS are set according to
#   $rpl_source_log_file and $rpl_source_log_pos
#
# Finally, it calls CHANGE REPLICATION SOURCE with the computed parameters.
#
# If the server was already configured (according to
# $_rpl_old_connection_list), then we skip executing this file.
#
# ==== Note ===
# This file uses $rpl_channel_name in CHANGE REPLICATION SOURCE command, which is
# set in rpl/for_each_connection.inc
#

--let $include_filename= rpl/impl/change_topology_helper.inc [rpl_master=$rpl_master rpl_slave=$rpl_slave]
--source include/begin_include_file.inc

# Determine if we really need to execute a CHANGE REPLICATION SOURCE.
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

  # Get GTID_ONLY for current `$rpl_slave`
  --let $_rpl_change_topology_helper_gtid_only_index= `SELECT LOCATE('$rpl_slave:', "$rpl_gtid_only")`
  --let $_rpl_change_topology_helper_gtid_only = 0

  if ($_rpl_change_topology_helper_gtid_only_index)
  {

    --let $_rpl_change_topology_helper_gtid_only= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING("$rpl_gtid_only", $_rpl_change_topology_helper_gtid_only_index), ',', 1), ':', -1)`
  }



  # Get GTID_ONLY for all replicas
  if (`SELECT $_rpl_change_topology_helper_gtid_only_index = 0 AND "$_rpl_change_topology_gtid_only_all" != ""`)
  {
    --let $_rpl_change_topology_helper_gtid_only= $_rpl_change_topology_gtid_only_all
  }

  # GTID_ONLY can only be enabled if GTID_MODE==1, SOURCE_AUTO_POSITION==1, REQUIRE_ROW_FORMAT==1
  # When GTID_ONLY is enabled and source-parallel-workers == 0 then source-parallel-workers is set to 1
  if ($_rpl_change_topology_helper_gtid_only == 1)
  {
    --source include/rpl/assert_gtid_mode_on.inc
    --let $use_gtids= 1
    --source include/have_binlog_format_row.inc
    --source include/have_replica_parallel_workers.inc
  }



  # Configure whether we use AUTO_POSITION.
  if ($use_gtids)
  {
    --source include/rpl/assert_gtid_mode_on.inc
    --let $_rpl_change_topology_helper_position= SOURCE_AUTO_POSITION = 1,
  }
  if (!$use_gtids)
  {
    --let $_rpl_change_topology_helper_position= SOURCE_AUTO_POSITION = 0,

    # Get SOURCE_LOG_FILE
    --let $_rpl_change_topology_helper_file_index= `SELECT LOCATE('$rpl_slave:', '$rpl_source_log_file')`
    if ($_rpl_change_topology_helper_file_index)
    {
      # Get text from after ':' and before ',', starting at
      # $_rpl_source_log_file
      --let $_rpl_change_topology_helper_source_log_file= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$rpl_source_log_file', $_rpl_change_topology_helper_file_index), ',', 1), ':', -1)`
      --let $_rpl_change_topology_helper_position= SOURCE_LOG_FILE = '$_rpl_source_log_file',
    }

    # Get SOURCE_LOG_POS
    --let $_rpl_change_topology_helper_pos_index= `SELECT LOCATE('$rpl_slave:', '$rpl_source_log_pos')`
    if ($_rpl_change_topology_helper_pos_index)
    {
      --let $_rpl_change_topology_helper_source_log_pos= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$rpl_source_log_pos', $_rpl_change_topology_helper_pos_index), ',', 1), ':', -1)`
      --let $_rpl_change_topology_helper_position= $_rpl_change_topology_helper_position SOURCE_LOG_POS = $_rpl_change_topology_helper_source_log_pos,
    }
  }

  --let $rpl_instance_privilege_checks_user= NULL
  if ($rpl_privilege_checks_user != ''){
    --let $rpl_server_number= $rpl_slave
    --source include/rpl/get_instance_priv_checks_user.inc
  }

  --let $_rpl_change_topology_helper_position= $_rpl_change_topology_helper_position PRIVILEGE_CHECKS_USER = $rpl_instance_privilege_checks_user,

  # If PRIVILEGE_CHECKS_USER is not NULL configure the account
  if (`SELECT "$rpl_instance_privilege_checks_user" != "NULL"`)
  {
    --source include/rpl/priv_checks_user_configuration.inc
  }

  # Get REQUIRE_ROW_FORMAT for current `$rpl_slave`
  --let $_rpl_change_topology_helper_require_row_index= `SELECT LOCATE('$rpl_slave:', "$rpl_require_row_format")`
  --let $_rpl_change_topology_helper_require_row = 0
  if ($_rpl_change_topology_helper_require_row_index)
  {
    --let $_rpl_change_topology_helper_require_row= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING("$rpl_require_row_format", $_rpl_change_topology_helper_require_row_index), ',', 1), ':', -1)`
  }
  # Get REQUIRE_ROW_FORMAT for all slaves
  if (`SELECT $_rpl_change_topology_helper_require_row_index = 0 AND "$_rpl_change_topology_require_row_all" != ""`)
  {
    --let $_rpl_change_topology_helper_require_row= $_rpl_change_topology_require_row_all
  }


  --let $_rpl_change_topology_helper_position= $_rpl_change_topology_helper_position REQUIRE_ROW_FORMAT = $_rpl_change_topology_helper_require_row,

  --let $_rpl_change_topology_helper_position= $_rpl_change_topology_helper_position GTID_ONLY = $_rpl_change_topology_helper_gtid_only,

  # Get REQUIRE_TABLE_PRIMARY_KEY_CHECK for current `$rpl_slave`
  --let $_rpl_change_topology_helper_require_table_primary_key_check_index= `SELECT LOCATE('$rpl_slave:', "$rpl_require_table_primary_key_check")`
  --let $_rpl_change_topology_helper_require_table_primary_key_check = -1
  if ($_rpl_change_topology_helper_require_table_primary_key_check_index)
  {
    --let $_rpl_change_topology_helper_require_table_primary_key_check= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING("$rpl_require_table_primary_key_check", $_rpl_change_topology_helper_require_table_primary_key_check_index), ',', 1), ':', -1)`
  }
  # Get _rpl_change_topology_helper_require_table_primary_key_check_index for all slaves
  if (`SELECT $_rpl_change_topology_helper_require_table_primary_key_check_index = 0 AND "$_rpl_require_table_primary_key_check_all" != ""`)
  {
    --let $_rpl_change_topology_helper_require_table_primary_key_check= $_rpl_require_table_primary_key_check_all
  }
  # Set it if defined
  if ($_rpl_change_topology_helper_require_table_primary_key_check != -1){
    --let $_rpl_change_topology_helper_position= $_rpl_change_topology_helper_position REQUIRE_TABLE_PRIMARY_KEY_CHECK = $_rpl_change_topology_helper_require_table_primary_key_check,
  }

  --replace_regex /[0-9]{4}/####/
    if (!$rpl_multi_source)
    {
      eval CHANGE REPLICATION SOURCE TO SOURCE_HOST = '127.0.0.1', SOURCE_PORT = $_rpl_change_topology_helper_port, SOURCE_USER = 'root', $_rpl_change_topology_helper_position SOURCE_CONNECT_RETRY = 1 FOR CHANNEL '';
    }
    if ($rpl_multi_source)
    {
      eval CHANGE REPLICATION SOURCE TO SOURCE_HOST = '127.0.0.1', SOURCE_PORT = $_rpl_change_topology_helper_port, SOURCE_USER = 'root', $_rpl_change_topology_helper_position SOURCE_CONNECT_RETRY = 1 FOR CHANNEL $rpl_channel_name;
    }

}

if ($_rpl_change_topology_helper_skip)
{
  if (!$use_gtids)
  {
    --let $_source_auto_position= query_get_value(SHOW REPLICA STATUS $rpl_for_channel_clause, Auto_Position, 1)
      if ($_source_auto_position == 1)
      {
        if (!$rpl_multi_source)
        {
          CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION= 0 FOR CHANNEL '';
        }
        if ($rpl_multi_source)
        {
          eval CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION=0 FOR CHANNEL $rpl_channel_name;
        }
      }
  }
}


if (!$_rpl_change_topology_helper_skip)
{
  # This un-configures the server so that it's not a slave.
  # After BUG#28796, such configuration is not possible any more.
  #CHANGE REPLICATION SOURCE TO SOURCE_HOST = '' FOR CHANNEL $rpl_channel_name;
}


--let $include_filename= rpl/impl/change_topology_helper.inc [rpl_master=$rpl_master rpl_slave=$rpl_slave]
--source include/end_include_file.inc
