

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@schema_id
}

if (!$mrs_modify_options)
{
  --die `mrs_modify_options` variable is required.
}

--disable_query_log
--disable_result_log

eval UPDATE `mysql_rest_service_metadata`.`db_schema` SET options = $mrs_modify_options WHERE `id`=$mrs_sql_id_variable;

--enable_query_log
--enable_result_log


--let $mrs_modify_options=
--let $mrs_sql_id_variable=
