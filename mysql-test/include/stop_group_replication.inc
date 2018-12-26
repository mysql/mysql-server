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
#   $wait_timeout
#     See include/wait_condition.inc
#
#   $rpl_debug
#     See include/rpl_init.inc

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

--let $include_filename= stop_group_replication.inc
--source include/end_include_file.inc
