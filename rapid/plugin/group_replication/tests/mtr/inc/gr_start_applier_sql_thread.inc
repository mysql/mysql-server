# ==== Purpose ====
#
# Start the group replication applier SQL thread and wait for it to start.
#
# ==== Usage ====
#
# [--let $wait_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source ../inc/gr_start_applier_sql_thread.inc
#

--let $include_filename= gr_start_applier_sql_thread.inc
--source include/begin_include_file.inc

START SLAVE SQL_THREAD FOR CHANNEL "group_replication_applier";
--let $wait_condition= SELECT service_state="ON" FROM performance_schema.replication_applier_status WHERE channel_name="group_replication_applier"
--source include/wait_condition.inc

if (!$success)
{
  --echo Current connection: $CURRENT_CONNECTION
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

--let $include_filename= gr_start_applier_sql_thread.inc
--source include/end_include_file.inc
