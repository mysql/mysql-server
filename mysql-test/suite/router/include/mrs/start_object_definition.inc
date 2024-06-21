if ($extra_mrs_router_id)
{
  --let $_mrs_definition_start=`SELECT NOW()`

  --let $wait_condition=SELECT last_check_in>"$_mrs_definition_start" FROM mysql_rest_service_metadata.router WHERE id=$extra_mrs_router_id;
  --source include/wait_condition_or_abort.inc

  --source changes/fetch_asof_ro_count.inc
  --source changes/fetch_asof_rw_count.inc
  --source changes/fetch_asof_switches_ro_to_rw_count.inc
  --source changes/fetch_asof_timeouts.inc
  --source changes/fetch_metadata_gtids.inc
  --source changes/fetch_files.inc
  --source changes/fetch_object.inc
}

--let $_mrs_object_definitions=0
--let $_mrs_files_definitions=0

--disable_query_log
--disable_result_log
START TRANSACTION;
--enable_query_log
--enable_result_log
