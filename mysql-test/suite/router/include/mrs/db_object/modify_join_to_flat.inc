
if (!$mrs_sql_id_variable_object_ref)
{
  --let $mrs_sql_id_variable_object_ref=@object_ref
}

if (!`SELECT $mrs_sql_id_variable_object_ref`)
{
  die \$mrs_sql_id_variable_object_ref must be set to a valid value.;
}

--disable_query_log
--disable_result_log
--echo # Modified OBJECT_REFERENCE to be an unnested filed.
eval UPDATE mysql_rest_service_metadata.object_reference SET 
               unnest=True WHERE id=$mrs_sql_id_variable_object_ref;
             
--enable_query_log
--enable_result_log

--let $mrs_sql_id_variable_object_ref=
