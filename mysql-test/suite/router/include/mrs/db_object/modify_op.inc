

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

--let $_mrs_data_provider_grant_schema=`SELECT s.name FROM mysql_rest_service_metadata.db_object o JOIN mysql_rest_service_metadata.db_schema s ON s.id=o.db_schema_id WHERE o.id=$mrs_sql_id_variable`
--let $_mrs_data_provider_grant_object=`SELECT name FROM mysql_rest_service_metadata.db_object WHERE id=$mrs_sql_id_variable`
--let $_mrs_data_provider_grant_type=`SELECT object_type FROM mysql_rest_service_metadata.db_object WHERE id=$mrs_sql_id_variable`
--let $_mrs_data_provider_grant_operations=$mrs_add_db_object_op
--source grant_data_provider.inc

if ($mrs_schema_version > 2)
{
  let $_object_options=`SELECT JSON_MERGE_PATCH(JSON_OBJECT(), IF(FIND_IN_SET("CREATE","$mrs_add_db_object_op"),'{"dataMappingViewInsert":true}',"{}"), IF(FIND_IN_SET("UPDATE","$mrs_add_db_object_op"),'{"dataMappingViewUpdate":true}',"{}"), IF(FIND_IN_SET("UPDATE","$mrs_add_db_object_op"),'{"dataMappingViewDelete":true}',"{}"))`;
  eval UPDATE `mysql_rest_service_metadata`.`object` SET options='$_object_options' WHERE db_object_id=$mrs_sql_id_variable;
}

--enable_query_log
--enable_result_log


--let $mrs_add_db_object_op=
--let $mrs_sql_id_variable=
