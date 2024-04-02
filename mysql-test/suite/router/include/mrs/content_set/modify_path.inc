

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@content_set_id
}

if (!$mrs_modify_path)
{
  --die `mrs_modify_path` variable is required.
}

--disable_query_log
--disable_result_log

eval UPDATE `mysql_rest_service_metadata`.`content_set` SET request_path = "$mrs_modify_path" WHERE `id`=$mrs_sql_id_variable;

--enable_query_log
--enable_result_log


--let $mrs_modify_path=
--let $mrs_sql_id_variable=
