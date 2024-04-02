if (!$mrs_sql_changes_variable)
{
  --let $mrs_sql_changes_variable=@file_changes
}

--disable_query_log
--disable_result_log

if (!$_mrs_not_first_file_fetch)
{
  eval SET $mrs_sql_changes_variable=0;
  if (!$mrs_do_not_wait_at_fetch)
  {
    --source ./wait_files.inc
  }
  --let $_mrs_not_first_file_fetch=1
}

--disable_query_log
--disable_result_log

eval SET $mrs_sql_changes_variable=(
           SELECT IF(v is NULL,NULL,v+0) FROM (SELECT details->>"$.changesFiles" as v
           FROM mysql_rest_service_metadata.router_status
           ORDER BY id DESC LIMIT 0,1)as sub);

--enable_query_log
--enable_result_log

--let $mrs_schema_id_sql_variable=
--let $mrs_sql_changes_variable=
--let $mrs_do_not_wait_at_fetch=
