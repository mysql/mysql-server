

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@db_object_id
}

if (!$mrs_modify_enabled)
{
  --die `mrs_modify_enabled` variable is required.
}

--disable_query_log
--disable_result_log

eval UPDATE `mysql_rest_service_metadata`.`db_object` SET enabled = $mrs_modify_enabled WHERE `id`=$mrs_sql_id_variable;

--enable_query_log
--enable_result_log


--let $mrs_modify_enabled=
--let $mrs_sql_id_variable=
