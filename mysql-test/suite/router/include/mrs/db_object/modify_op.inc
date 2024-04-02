

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@db_object_id
}

if (!$mrs_add_db_object_op)
{
  --die `mrs_add_db_object_op` variable is required.
}

--disable_query_log
--disable_result_log

eval UPDATE `mysql_rest_service_metadata`.`db_object` SET crud_operations = "$mrs_add_db_object_op" WHERE `id`=$mrs_sql_id_variable;

--enable_query_log
--enable_result_log


--let $mrs_add_db_object_op=
--let $mrs_sql_id_variable=
