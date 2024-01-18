# ==== Purpose ====
#
# Issues STOP GROUP_REPLICATION on the current connection. Then waits
# until member state is OFFLINE.
#
#
# ==== Usage ====
#
# --let $group_replication_group_name= UUID
# [--let $skip_assert_read_only= 1]
# [--let $skip_reset_read_mode= 1]
# [--let $skip_assert_offline_mode= 1]
# [--let $skip_reset_offline_mode= 1]
# [--let $$wait_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/stop_group_replication.inc
#
# Parameters:
#   $group_replication_group_name
#     Group name
#
#   $skip_assert_read_only
#     Skip assert that super_read_only and read_only variables are enabled, and
#     the consequent disable of both variables
#
#   $skip_reset_read_mode
#     Skip the disabling of read_only vars
#     See include/assert_and_disable_read_only.inc
#
#   $skip_assert_offline_mode
#     Skip assert that offline_mode is enabled when the member is in ERROR state,
#     and the consequent disable of both variables
#
#   $skip_reset_offline_mode
#     Skip reset offline_mode variable.
#
#   $wait_timeout
#     See include/wait_condition.inc
#
#   $rpl_debug
#     See include/rpl/init.inc

--let $include_filename= stop_group_replication.inc
--source include/begin_include_file.inc

if ($group_replication_group_name == '')
{
  --die ERROR IN TEST: the "group_replication_group_name" variable must be set
}

if (!$rpl_debug)
{
  --disable_query_log
}

--let $_is_member_in_error_state=
--let $_is_member_auto_rejoin_running= 0
if (!$skip_assert_offline_mode)
{
  # Save member state before it changes to OFFLINE.
  --let $_this_member_id= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)
  --let $_is_member_in_error_state= `SELECT member_state IN ("ERROR") FROM performance_schema.replication_group_members WHERE member_id="$_this_member_id"`

  if ($_is_member_in_error_state)
  {
    # Check if auto-rejoin is running before STOP.
    --let $_is_member_auto_rejoin_running= `SELECT COUNT(*) FROM performance_schema.events_stages_current WHERE EVENT_NAME LIKE '%auto-rejoin%'`
  }
}

STOP GROUP_REPLICATION;

--let $wait_condition= SELECT COUNT(*)=1 FROM performance_schema.replication_connection_status connection_status WHERE connection_status.channel_name="group_replication_applier" AND connection_status.service_state="OFF"
--source include/wait_condition.inc
if (!$success)
{
  --echo Current connection: $CURRENT_CONNECTION

  --echo # performance_schema.replication_connection_status
  --query_vertical SELECT * FROM performance_schema.replication_connection_status

  --echo # performance_schema.replication_group_members
  --query_vertical SELECT * FROM performance_schema.replication_group_members

  --echo # performance_schema.replication_group_member_stats
  --query_vertical SELECT * FROM performance_schema.replication_group_member_stats

  --die Timeout in wait_condition.inc
}

if (!$skip_assert_read_only)
{
  --source include/assert_and_disable_read_only.inc
}
--let $skip_assert_read_only=

if (!$skip_assert_offline_mode)
{
  if ($_is_member_in_error_state)
  {
    if ($_is_member_auto_rejoin_running == 0)
    {
      --let $_member_exit_state_action= `SELECT @@GLOBAL.group_replication_exit_state_action`
      if ($_member_exit_state_action == "OFFLINE_MODE")
      {
        --source include/assert_and_disable_offline_mode.inc
      }
    }
  }
}
--let $skip_assert_offline_mode=

--let $include_filename= stop_group_replication.inc
--source include/end_include_file.inc
