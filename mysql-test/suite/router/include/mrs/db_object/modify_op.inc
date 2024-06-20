

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

if ($mrs_schema_version == 3)
{
  let $_object_options=`SELECT JSON_MERGE_PATCH(JSON_OBJECT(), IF(FIND_IN_SET("CREATE","$mrs_add_db_object_op"),'{"duality_view_insert":true}',"{}"), IF(FIND_IN_SET("UPDATE","$mrs_add_db_object_op"),'{"duality_view_update":true}',"{}"), IF(FIND_IN_SET("UPDATE","$mrs_add_db_object_op"),'{"duality_view_delete":true}',"{}"))`;
  eval UPDATE `mysql_rest_service_metadata`.`object` SET options='$_object_options' WHERE db_object_id=$mrs_sql_id_variable;
}

--enable_query_log
--enable_result_log


--let $mrs_add_db_object_op=
--let $mrs_sql_id_variable=
