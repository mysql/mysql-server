

if (!$mrs_add_host_name)
{
  --die 'mrs_add_host_name' variable is required.
}


if (!$mrs_sql_id_variable)
{
  --die 'mrs_sql_id_variable' variable is required.
}

--disable_query_log
--disable_result_log
eval SET $mrs_sql_id_variable=mysql_rest_service_metadata.get_sequence_id();
eval  INSERT INTO mysql_rest_service_metadata.url_host(id, name)
      VALUES($mrs_sql_id_variable, $mrs_add_host_name);
--enable_query_log
--enable_result_log

--let $mrs_add_host_name=
