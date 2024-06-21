if (!$mrs_sql_changes_variable)
{
  --let $mrs_sql_changes_variable=@file_changes
}

if (!$mrs_changes_current_is)
{
  --let $mrs_changes_current_is= > $mrs_sql_changes_variable
  if ($mrs_changes_inc)
  {
    --let $mrs_changes_current_is= = ($mrs_sql_changes_variable+$mrs_changes_inc)
  }
}


--disable_query_log
--disable_result_log

--let $old_value=`SELECT $mrs_sql_changes_variable`
--let $wait_condition=SELECT details->>"$.changesFiles" $mrs_changes_current_is FROM mysql_rest_service_metadata.router_status ORDER BY id DESC LIMIT 0,1;
--source include/wait_condition_or_abort.inc

--enable_query_log
--enable_result_log

--source ./fetch_files.inc

if ($ROUTER_MRS_DEBUG==1)
{
  --let $new_value=`SELECT $_mrs_sql_changes_variable_after_fetch`
  --echo Wait files - old limit: $old_value, new limit: $new_value
}

--let $mrs_changes_inc=
--let $mrs_changes_current_is=
--let $mrs_schema_id_sql_variable=
if ($_mrs_not_first_file_fetch==1)
{
--let $mrs_sql_changes_variable=
}
