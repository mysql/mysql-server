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

--let $include_filename= gr_start_applier_sql_thread.inc
--source include/end_include_file.inc
