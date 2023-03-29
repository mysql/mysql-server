

if (!$mrs_schema_id_sql_variable)
{
  --let $mrs_schema_id_sql_variable=@schema_id
}

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@db_object_id
}

if (!$mrs_add_db_object)
{
  --die 'mrs_add_db_object' variable is required.
}

if (!$mrs_add_db_object_path)
{
  --let $mrs_add_db_object_path=/$mrs_add_db_object
}

if (!$mrs_add_db_object_auth)
{
  --let $mrs_add_db_object_auth=DEFAULT
}

if (!$mrs_add_db_object_type)
{
  --let $mrs_add_db_object_type=TABLE
}

if (!$mrs_add_db_object_op)
{
  --let $mrs_add_db_object_op=CREATE,READ,UPDATE,DELETE
}

if (!$mrs_add_db_object_format)
{
  --let $mrs_add_db_object_format=FEED
}

if (!$mrs_add_db_object_items_on_page)
{
  --let $mrs_add_db_object_items_on_page=DEFAULT
}

if (!$mrs_add_db_object_options)
{
  --let $mrs_add_db_object_options=DEFAULT
}

--disable_query_log
--disable_result_log
eval SET $mrs_sql_id_variable=mysql_rest_service_metadata.get_sequence_id();
eval  INSERT INTO mysql_rest_service_metadata.db_object(
               id, object_type, db_schema_id, name, request_path,
               requires_auth, items_per_page, options, crud_operations, format $mrs_add_db_object_custom_fields)
      VALUES($mrs_sql_id_variable, "$mrs_add_db_object_type", $mrs_schema_id_sql_variable,
             "$mrs_add_db_object", "$mrs_add_db_object_path",
             $mrs_add_db_object_auth, $mrs_add_db_object_items_on_page,
             $mrs_add_db_object_options, "$mrs_add_db_object_op", "$mrs_add_db_object_format" $mrs_add_db_object_custom_values);
             
--let $_path=`SELECT CONCAT(s.url_context_root, db.request_path, o.request_path) FROM mysql_rest_service_metadata.db_object as o JOIN mysql_rest_service_metadata.db_schema as db on db.id=o.db_schema_id JOIN mysql_rest_service_metadata.service as s ON  s.id = db.service_id  WHERE o.id=$mrs_sql_id_variable;`
--echo # Registred DB_OBJECT at path: $_path
--enable_query_log
--enable_result_log


--let $mrs_schema_id_sql_variable=
--let $mrs_sql_id_variable=
--let $mrs_add_db_object_type=
--let $mrs_add_db_object=
--let $mrs_add_db_object_path=
--let $mrs_add_db_object_auth=
--let $mrs_add_db_object_items_on_page=
--let $mrs_add_db_object_options=
--let $mrs_add_db_object_op=
--let $mrs_add_db_object_format=
--let $mrs_add_db_object_custom_values=


