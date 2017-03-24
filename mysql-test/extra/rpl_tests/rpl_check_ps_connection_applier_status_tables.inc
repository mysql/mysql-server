# This file is used by extra/rpl_tests/rpl_ps_connection_applier_status.inc
#
# It checks that the monitoring info associated with a GTID event is correctly
# reported in each of the corresponding replication P_S tables.
#
# params: $current_gtid  the gtid that is currently being processed
#         $last_gtid     the gtid that was last processed
#         $is_mts        [0,1] 1 if MTS test

--source include/rpl_connection_slave.inc
--let $debug_point= rpl_ps_tables_queue
--source include/add_debug_point.inc
--source include/start_slave_io.inc

--let $table= performance_schema.replication_connection_status
--let $now_handling_gtid_column= QUEUEING_TRANSACTION
--let $now_handling_OCT_column= QUEUEING_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP
--let $now_handling_ICT_column= QUEUEING_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP
--let $now_handling_start_column= QUEUEING_TRANSACTION_START_QUEUE_TIMESTAMP
--let $last_handled_gtid_column= LAST_QUEUED_TRANSACTION
--let $last_handled_OCT_column= LAST_QUEUED_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP
--let $last_handled_ICT_column= LAST_QUEUED_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP
--let $last_handled_start_column= LAST_QUEUED_TRANSACTION_START_QUEUE_TIMESTAMP
--let $last_handled_end_column= LAST_QUEUED_TRANSACTION_END_QUEUE_TIMESTAMP
--let $debug_sync_1= 'now WAIT_FOR signal.rpl_ps_tables_queue_before'
--let $debug_sync_2= 'now SIGNAL signal.rpl_ps_tables_queue_finish'
--let $debug_sync_3= 'now WAIT_FOR signal.rpl_ps_tables_queue_after_finish'
--let $debug_sync_4= 'now SIGNAL signal.rpl_ps_tables_queue_continue'
--let $now_handling_action= queueing
--let $last_handled_action= queued
--let $is_first_check= 1
--source extra/rpl_tests/rpl_check_ps_connection_applier_status_tables_asserts.inc

if ($is_mts)
{
  --let $table= performance_schema.replication_applier_status_by_coordinator
  --let $now_handling_gtid_column= PROCESSING_TRANSACTION
  --let $now_handling_OCT_column= PROCESSING_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP
  --let $now_handling_ICT_column= PROCESSING_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP
  --let $now_handling_start_column= PROCESSING_TRANSACTION_START_BUFFER_TIMESTAMP
  --let $last_handled_gtid_column= LAST_PROCESSED_TRANSACTION
  --let $last_handled_OCT_column= LAST_PROCESSED_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP
  --let $last_handled_ICT_column= LAST_PROCESSED_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP
  --let $last_handled_start_column= LAST_PROCESSED_TRANSACTION_START_BUFFER_TIMESTAMP
  --let $last_handled_end_column= LAST_PROCESSED_TRANSACTION_END_BUFFER_TIMESTAMP
  --let $debug_sync_1= 'now WAIT_FOR signal.rpl_ps_tables_process_before'
  --let $debug_sync_2= 'now SIGNAL signal.rpl_ps_tables_process_finish'
  --let $debug_sync_3= 'now WAIT_FOR signal.rpl_ps_tables_process_after_finish'
  --let $debug_sync_4= 'now SIGNAL signal.rpl_ps_tables_process_continue'
  --let $now_handling_action= processing
  --let $last_handled_action= processed
  --let $is_first_check= 0
  --source extra/rpl_tests/rpl_check_ps_connection_applier_status_tables_asserts.inc
}

if ($is_first_stmt)
{
--let $last_gtid= $last_gtid_applied
}
--let $table= performance_schema.replication_applier_status_by_worker
--let $now_handling_gtid_column= APPLYING_TRANSACTION
--let $now_handling_OCT_column= APPLYING_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP
--let $now_handling_ICT_column= APPLYING_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP
--let $now_handling_start_column= APPLYING_TRANSACTION_START_APPLY_TIMESTAMP
--let $last_handled_gtid_column= LAST_APPLIED_TRANSACTION
--let $last_handled_OCT_column= LAST_APPLIED_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP
--let $last_handled_ICT_column= LAST_APPLIED_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP
--let $last_handled_start_column= LAST_APPLIED_TRANSACTION_START_APPLY_TIMESTAMP
--let $last_handled_end_column= LAST_APPLIED_TRANSACTION_END_APPLY_TIMESTAMP
--let $debug_sync_1= 'now WAIT_FOR signal.rpl_ps_tables_apply_before'
--let $debug_sync_2= 'now SIGNAL signal.rpl_ps_tables_apply_finish'
--let $debug_sync_3= 'now WAIT_FOR signal.rpl_ps_tables_apply_after_finish'
--let $debug_sync_4= 'now SIGNAL signal.rpl_ps_tables_apply_continue'
--let $now_handling_action= applying
--let $last_handled_action= applied
--let $is_first_check= 0
--source extra/rpl_tests/rpl_check_ps_connection_applier_status_tables_asserts.inc
