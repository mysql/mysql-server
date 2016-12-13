# ==== Purpose ====
#
# Configure N servers to be in single primary mode.
#
# It does not change the current connection.
#
# ==== Usage ====
#
#   --let $rpl_server_count= N
#   [--let $reset_single_primary_mode=1|0]
#   --source ../inc/single_primary_mode.inc
#
# Parameters:
#   $rpl_server_count
#     See include/master-slave.inc
#
#   $reset_single_primary_mode
#     To check whether we are going to set or
#     reset the single primary mode configuration
#

--let $include_filename= single_primary_mode.inc
--source include/begin_include_file.inc

if (!$reset_single_primary_mode)
{
  --let $reset_single_primary_mode= 0
}

--let $_i_set_single_primary_mode= 1
--let $_saved_connection= $CURRENT_CONNECTION

--disable_query_log

while ($_i_set_single_primary_mode <= $rpl_server_count)
{
  --let $rpl_connection_name= server$_i_set_single_primary_mode
  --source include/rpl_connection.inc

  # plugin cannot be running for us to setup these configs
  let $_this_member_id=
    query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1);
  let $_is_plugin_running=
    `SELECT member_state IN ("ONLINE", "UNREACHABLE", "RECOVERY", "ERROR")
       FROM performance_schema.replication_group_members
      WHERE member_id="$_this_member_id"`;

  if ($_is_plugin_running)
  {
    --die "Cannot set/reset single primary mode while the plugin is running!"
  }

  if (!$reset_single_primary_mode)
  {
    SET GLOBAL group_replication_enforce_update_everywhere_checks= FALSE;
    SET GLOBAL group_replication_single_primary_mode= TRUE;
  }

  if ($reset_single_primary_mode)
  {
    SET GLOBAL group_replication_single_primary_mode= FALSE;
    SET GLOBAL group_replication_enforce_update_everywhere_checks= TRUE;
  }

  --inc $_i_set_single_primary_mode
}

--enable_query_log

--let $reset_single_primary_mode=

--connection $_saved_connection

--let $include_filename= single_primary_mode.inc
--source include/end_include_file.inc
