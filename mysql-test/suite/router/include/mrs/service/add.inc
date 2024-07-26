
# $mrs_add_service_options
# $mrs_add_service_protocol
# $mrs_add_service_path
# $mrs_add_service_custom_fields
# $mrs_add_service_custom_values
# $mrs_add_service_in_development
# $mrs_sql_id_variable
# $mrs_host_id
# $mrs_add_host_name


if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@service_id
}

if (!$mrs_host_id)
{
  if (!$mrs_add_host_name)
  {
    --die Specify HOST either by creating it by HostName or by specifing its ID.
  }
}

if (!$mrs_add_service_path)
{
  --die 'mrs_add_service_path' variable is required.
}

if (!$mrs_add_service_options)
{
  #--let $mrs_add_service_options=NULL
  --let $mrs_add_service_options='{"logging":{"exceptions":true}}'
}

if (!$mrs_add_service_protocol)
{
    --let $mrs_add_service_protocol='HTTPS'
}

--let $mrs_add_service_published=0
if (!$mrs_add_service_in_development)
{
  --let $mrs_add_service_in_development=NULL
  --let $mrs_add_service_published=1
}

if ($mrs_add_host_name)
{
   --let $mrs_get_host_name=$mrs_add_host_name
   --source ../host/get_host.inc
}

if (!$mrs_host_id)
{
   --let $mrs_old_sql_id=$mrs_sql_id_variable
   --let $mrs_sql_id_variable=@temporary_host_id
   --source ../host/add_host.inc
   --let $mrs_host_id=query_get_value(SELECT HEX($mrs_sql_id_variable) as c1, c1, 1)
   --let $mrs_sql_id_variable=$mrs_old_sql_id
}

--disable_query_log
--disable_result_log
eval SET $mrs_sql_id_variable=mysql_rest_service_metadata.get_sequence_id();

if ($mrs_schema_version == 2)
{
  eval INSERT INTO mysql_rest_service_metadata.service(id, url_host_id, url_context_root, url_protocol, options $mrs_add_service_custom_fields)
    VALUES($mrs_sql_id_variable, unhex("$mrs_host_id"),  $mrs_add_service_path, $mrs_add_service_protocol, $mrs_add_service_options $mrs_add_service_custom_values);
}

if ($mrs_schema_version == 3)
{
  eval INSERT INTO mysql_rest_service_metadata.service(id, url_host_id, url_context_root, url_protocol, options, published, in_development $mrs_add_service_custom_fields)
    VALUES($mrs_sql_id_variable, unhex("$mrs_host_id"),  $mrs_add_service_path, $mrs_add_service_protocol, $mrs_add_service_options, $mrs_add_service_published, $mrs_add_service_in_development $mrs_add_service_custom_values);
}
  
--let $_path=`SELECT CONCAT(s.url_context_root) FROM mysql_rest_service_metadata.service as s  WHERE s.id=$mrs_sql_id_variable;`
--echo # Registred SERVICE at path: $_path
  
--enable_query_log
--enable_result_log


--let $mrs_add_host_name=
--let $mrs_host_id=
--let $mrs_add_service_options=
--let $mrs_add_service_protocol=
--let $mrs_add_service_path=
--let $mrs_add_service_published=
--let $mrs_add_service_in_development=
--let $mrs_add_service_custom_fields=
--let $mrs_add_service_custom_values=
--let $mrs_sql_id_variable=
