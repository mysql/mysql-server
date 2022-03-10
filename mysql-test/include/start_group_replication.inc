# ==== Purpose ====
#
# Issues START GROUP_REPLICATION on the current connection. Then waits
# until member state is ONLINE or the specified by
# $group_replication_start_member_state.
#
#
# ==== Usage ====
#
# --let $group_replication_group_name= UUID
# [--let $group_replication_start_member_state= STATE]
# [--let $skip_wait_for_disabled_read_only= 0]
# [--let $$wait_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/start_group_replication.inc
#
# Parameters:
#   $group_replication_group_name
#     Group name
#
#   $group_replication_start_member_state
#     The state that the member must have after start, ONLINE
#     if not specified
#
#   $skip_wait_for_disabled_read_only
#     Skip the wait for super_read_only and read_only variables are disabled
#
#   $wait_timeout
#     See include/wait_condition.inc
#
#   $rpl_debug
#     See include/rpl_init.inc

--let $include_filename= start_group_replication.inc
--source include/begin_include_file.inc

if ($group_replication_group_name == '')
{
  --die ERROR IN TEST: the "group_replication_group_name" variable must be set
}

if (!$group_replication_start_member_state)
{
  --let $group_replication_start_member_state= ONLINE
}

if (!$rpl_debug)
{
  --disable_query_log
}

--eval SET GLOBAL group_replication_group_name= "$group_replication_group_name"

--source include/start_group_replication_command.inc

--let $_member_id= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)
--let $wait_condition= SELECT COUNT(*)=1 FROM performance_schema.replication_group_members group_members WHERE group_members.channel_name="group_replication_applier" AND group_members.member_id="$_member_id" AND group_members.member_state="$group_replication_start_member_state"
--source include/wait_condition.inc
if (!$success)
{
  --echo Current connection: $CURRENT_CONNECTION
  --echo group_replication_group_name= "$group_replication_group_name"
  --echo group_replication_member_id= "$_member_id"
  --echo Expected group_replication_member_state= "$group_replication_start_member_state"
  --echo
  --echo performance_schema.replication_connection_status
  --echo ------------------------------------------------
  --query_vertical SELECT * FROM performance_schema.replication_connection_status
  --echo
  --echo performance_schema.replication_applier_status
  --echo ---------------------------------------------
  --query_vertical SELECT * FROM performance_schema.replication_applier_status
  --echo
  --echo performance_schema.replication_applier_status_by_coordinator
  --echo ------------------------------------------------------------
  --query_vertical SELECT * FROM performance_schema.replication_applier_status_by_coordinator
  --echo
  --echo performance_schema.replication_applier_status_by_worker
  --echo -------------------------------------------------------
  --query_vertical SELECT * FROM performance_schema.replication_applier_status_by_worker
  --echo
  --echo performance_schema.replication_group_members
  --echo --------------------------------------------
  --query_vertical SELECT * FROM performance_schema.replication_group_members
  --echo
  --echo performance_schema.replication_group_member_stats
  --echo -------------------------------------------------
  --query_vertical SELECT * FROM performance_schema.replication_group_member_stats
  --die Timeout in wait_condition.inc on start_group_replication.inc
}

# Due to notify first the change to state ONLINE and after disable super read only
# when the lowest version servers are online we have two cases that need to
# wait for super read only= 0:
#   - when we are starting the primary in the single primary mode
#   - when we are starting a server on multiple primary

if (!$skip_wait_for_disabled_read_only)
{
  --let $_group_member_lowest_version= `SELECT member_version FROM performance_schema.replication_group_members ORDER BY member_version ASC LIMIT 1`

  if (`SELECT COUNT(*)=1 FROM performance_schema.replication_group_members group_members WHERE group_members.member_id= "$_member_id" AND group_members.member_state="ONLINE" AND group_members.member_role = "PRIMARY" AND group_members.member_version = "$_group_member_lowest_version"`)
  {
       --let $wait_condition= SELECT @@GLOBAL.read_only = '0'
       --source include/wait_condition_or_abort.inc
  }
}
--let $skip_wait_for_disabled_read_only=

--let $group_replication_start_member_state=

--let $include_filename= start_group_replication.inc
--source include/end_include_file.inc
