

if (!$mrs_service_id_sql_variable)
{
  --let $mrs_service_id_sql_variable=@service_id
}

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@schema_id
}

if (!$mrs_add_schema)
{
  --die 'mrs_add_schema' variable is required.
}

if (!$mrs_add_schema_path)
{
  --let $mrs_add_schema_path=/$mrs_add_schema
}

if (!$mrs_add_schema_auth)
{
  --let $mrs_add_schema_auth=DEFAULT
}


if (!$mrs_add_schema_items_on_page)
{
  --let $mrs_add_schema_items_on_page=DEFAULT
}

if (!$mrs_add_schema_options)
{
  --let $mrs_add_schema_options=DEFAULT
}

--disable_query_log
--disable_result_log

eval SET $mrs_sql_id_variable=mysql_rest_service_metadata.get_sequence_id();
eval  INSERT INTO mysql_rest_service_metadata.db_schema(
               id, service_id, name, request_path,
               requires_auth, items_per_page, options)
      VALUES($mrs_sql_id_variable, $mrs_service_id_sql_variable,
             "$mrs_add_schema", "$mrs_add_schema_path",
             $mrs_add_schema_auth, $mrs_add_schema_items_on_page,
             $mrs_add_schema_options);

--let $_path=`SELECT CONCAT(s.url_context_root, db.request_path) FROM mysql_rest_service_metadata.db_schema as db JOIN mysql_rest_service_metadata.service as s ON  s.id = db.service_id  WHERE db.id=$mrs_sql_id_variable;`
--echo # Registred DB_SCHEMA at path: $_path

--enable_query_log
--enable_result_log

--let $mrs_add_host_name=
--let $mrs_sql_id_variable=
--let $mrs_service_id_sql_variable=
--let $mrs_add_schema_auth=
