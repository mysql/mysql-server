

if (!$mrs_service_id_sql_variable)
{
  --let $mrs_service_id_sql_variable=@service_id
}

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@content_set_id
}

if (!$mrs_add_content_set_path)
{
  --die 'mrs_add_content_set_path' variable is required.
}

if (!$mrs_add_content_set_auth)
{
  --let $mrs_add_content_set_auth=DEFAULT
}

if (!$mrs_add_content_set_options)
{
  --let $mrs_add_content_set_options=DEFAULT
}

if (!$mrs_add_content_set_enabled)
{
  --let $mrs_add_content_set_enabled=DEFAULT
}


--disable_query_log
--disable_result_log

eval SET $mrs_sql_id_variable=mysql_rest_service_metadata.get_sequence_id();
eval  INSERT INTO mysql_rest_service_metadata.content_set(
               id, service_id, request_path,
               requires_auth, enabled, options)
      VALUES($mrs_sql_id_variable, $mrs_service_id_sql_variable,
             "$mrs_add_content_set_path",
             $mrs_add_content_set_auth,
             $mrs_add_content_set_enabled,
             $mrs_add_content_set_options);

--let $_path=`SELECT CONCAT(s.url_context_root, db.request_path) FROM mysql_rest_service_metadata.content_set as db JOIN mysql_rest_service_metadata.service as s ON  s.id = db.service_id  WHERE db.id=$mrs_sql_id_variable;`
--echo # Registred CONTENT_SET at path: $_path

--enable_query_log
--enable_result_log

--let $mrs_add_host_name=
--let $mrs_sql_id_variable=
--let $mrs_service_id_sql_variable=
--let $mrs_add_content_set_path=
--let $mrs_add_content_set_auth=
--let $mrs_add_content_set_options=
--let $mrs_add_content_set_enabled=
