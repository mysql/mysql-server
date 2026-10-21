if ($extra_mrs_router_id)
{
  --source wait_mrs_read_metadata.inc

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
--let $_mrs_data_provider_grants=
--let $_mrs_object_definition_transaction=1

--disable_query_log
--disable_result_log
START TRANSACTION;
--enable_query_log
--enable_result_log
