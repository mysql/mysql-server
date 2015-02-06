# ==== Purpose ====
#
# Issues START GROUP_REPLICATION on the current connection. Then waits
# until member state is ONLINE.
#
#
# ==== Usage ====
#
# --let $group_replication_group_name= UUID
# [--let $$wait_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/start_group_replication.inc
#
# Parameters:
#   $group_replication_group_name
#     Group name
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

if (!$rpl_debug)
{
  --disable_query_log
}

--eval SET GLOBAL group_replication_group_name= "$group_replication_group_name"
START GROUP_REPLICATION;

--let $_member_id= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)
--let $wait_condition= SELECT COUNT(*)=1 FROM performance_schema.replication_group_members group_members WHERE group_members.channel_name="group_replication_applier" AND group_members.member_id="$_member_id" AND group_members.member_state="ONLINE"
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

--let $include_filename= start_group_replication.inc
--source include/end_include_file.inc
