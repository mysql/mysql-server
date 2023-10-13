if (!$mrs_sql_changes_variable)
{
  --let $mrs_sql_changes_variable=@file_changes
}

if (!$mrs_changes_current_is)
{
  --let $mrs_changes_current_is= > $mrs_sql_changes_variable
}

--disable_query_log
--disable_result_log

--let $wait_condition=SELECT details->>"$.changesFiles" $mrs_changes_current_is FROM mysql_rest_service_metadata.router_status ORDER BY id DESC LIMIT 0,1;
--source include/wait_condition_or_abort.inc

--enable_query_log
--enable_result_log

if ($_mrs_not_first_file_fetch)
{
  --source ./fetch_files.inc
}

--let $mrs_changes_current_is=
--let $mrs_schema_id_sql_variable=
