# ==== Purpose ====
#
# Issues START GCS_REPLICATION on the current connection. Then waits
# until node state is ONLINE.
#
#
# ==== Usage ====
#
# --let $gcs_group_name= UUID
# [--let $$wait_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/start_gcs_replication.inc
#
# Parameters:
#   $gcs_group_name
#     Group name
#
#   $wait_timeout
#     See include/wait_condition.inc
#
#   $rpl_debug
#     See include/rpl_init.inc

--let $include_filename= start_gcs_replication.inc
--source include/begin_include_file.inc

if ($gcs_group_name == '')
{
  --die ERROR IN TEST: the "gcs_group_name" variable must be set
}

if (!$rpl_debug)
{
  --disable_query_log
}

--eval SET GLOBAL gcs_replication_plugin_group_name= "$gcs_group_name"
START GCS_REPLICATION;

--let $_node_id= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)
--let $wait_condition= SELECT COUNT(*)=1 FROM performance_schema.replication_group_members group_member WHERE group_member.member_id="$_node_id" AND group_member.member_state="ONLINE"
--source include/wait_condition.inc
if (!$success)
{
  --echo Current connection: $CURRENT_CONNECTION
  --query_vertical SELECT * FROM performance_schema.replication_connection_status
  --query_vertical SELECT * FROM performance_schema.replication_group_members
  --die Timeout in wait_condition.inc
}

--let $include_filename= start_gcs_replication.inc
--source include/end_include_file.inc
