# ==== Purpose ====
#
# Wait for the node on this server to reach a given state on the given group
#
#
# ==== Usage ====
#
# --let $gcs_group_name= UUID
# --let $gcs_node_state= STATE
# [--let $gcs_node_id= UUID]
# [--let $wait_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/rpl_gcs_wait_for_node_state.inc
#
# Parameters:
#   $gcs_group_name
#     Group name
#
#   $gcs_node_state
#     The state that the node should have
#
#   $gcs_node_id
#     The node uuid that should have the given state
#     When not present, the test is done based on the current selected node uuid
#
#   $wait_timeout
#     See include/wait_condition.inc
#
#   $rpl_debug
#     See include/rpl_init.inc

--let $include_filename= rpl_gcs_wait_for_node_state.inc
--source include/begin_include_file.inc

if ($gcs_group_name == '')
{
  --die ERROR IN TEST: the "gcs_group_name" variable must be set
}

if ($gcs_node_state == '')
{
  --die ERROR IN TEST: the "gcs_node_state" variable must be set
}

if (!$rpl_debug)
{
  --disable_query_log
}

--let $_node_id= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)
if ($gcs_node_id != "")
{
  --let $_node_id= $gcs_node_id
}

let $wait_condition= SELECT COUNT(*)=1 FROM performance_schema.replication_group_members
  WHERE member_id="$_node_id" AND
  member_state="$gcs_node_state";
--source include/wait_condition.inc
if (!$success)
{
  --query_vertical SELECT * FROM performance_schema.replication_connection_status
  --query_vertical SELECT * FROM performance_schema.replication_group_members
  --die Timeout in wait_condition.inc
}


--let $include_filename= rpl_gcs_wait_for_node_state.inc
--source include/end_include_file.inc
