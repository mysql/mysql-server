# ==== Purpose ====
#
# Wait for the given server to be elected as the Primary member of the group
#
# ==== Usage ====
#
# [--let $wait_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --let $group_replication_expected_uuid=UUID
# --source ../inc/gr_wait_primary_member_uuid.inc
#

--let $include_filename= gr_wait_primary_member_uuid.inc
--source include/begin_include_file.inc

let $wait_condition= SELECT COUNT(*) = 1 FROM performance_schema.global_status
   WHERE VARIABLE_NAME = "group_replication_primary_member" AND
         VARIABLE_VALUE = "$group_replication_expected_uuid";
--source include/wait_condition_or_abort.inc

if (!$success)
{
  --echo Current connection: $CURRENT_CONNECTION
  --echo group_replication_group_name= "$group_replication_group_name"
  --echo group_replication_expected_uuid expected= "$group_replication_expected_uuid"
  --echo
  --echo -------------------------------------------------
  --query_vertical SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME = "group_replication_primary_member"
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
  --die Timeout in wait_condition.inc due to incorrect expected member state
}

--let $include_filename= gr_wait_primary_member_uuid.inc
--source include/end_include_file.inc

--let $group_replication_expected_uuid=
--let $group_replication_member_role=
