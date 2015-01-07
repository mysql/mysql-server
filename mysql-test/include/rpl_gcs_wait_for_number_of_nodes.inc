# ==== Purpose ====
#
# Wait for the number of nodes on the given group name on this server.
#
#
# ==== Usage ====
#
# --let $gcs_group_name= UUID
# --let $gcs_number_of_nodes= NUMBER
# [--let $$wait_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/rpl_gcs_wait_for_number_of_nodes.inc
#
# Parameters:
#   $gcs_group_name
#     Group name
#
#   $gcs_number_of_nodes
#     Number of nodes that must be on the group.
#
#   $wait_timeout
#     See include/wait_condition.inc
#
#   $rpl_debug
#     See include/rpl_init.inc

--let $include_filename= rpl_gcs_wait_for_number_of_nodes.inc
--source include/begin_include_file.inc

if ($gcs_group_name == '')
{
  --die ERROR IN TEST: the "gcs_group_name" variable must be set
}

if ($gcs_number_of_nodes == '')
{
  --die ERROR IN TEST: the "gcs_number_of_nodes" variable must be set
}

if (!$rpl_debug)
{
  --disable_query_log
}

--let $wait_condition= SELECT COUNT(*)=$gcs_number_of_nodes FROM performance_schema.replication_group_members
if($gcs_number_of_nodes == 0)
{
  --let $wait_condition= SELECT COUNT(*)=1 FROM performance_schema.replication_group_members WHERE member_state = "OFFLINE"
}

--source include/wait_condition.inc
if (!$success)
{
  --echo Current connection: $CURRENT_CONNECTION
  --query_vertical SELECT * FROM performance_schema.replication_connection_status
  --query_vertical SELECT * FROM performance_schema.replication_group_members
  --die Timeout in wait_condition.inc due to lack of required members
}

--let $include_filename= rpl_gcs_wait_for_number_of_nodes.inc
--source include/end_include_file.inc
