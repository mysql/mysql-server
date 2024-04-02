
if (!$mrs_schema_id_sql_variable)
{
  --let $mrs_schema_id_sql_variable=@schema_id
}

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@db_object_id
}

if ($mrs_add_db_object_table_filter)
{
  --let $mrs_add_db_object_table_filter= AND $mrs_add_db_object_table_filter
}

if (!$mrs_add_db_object_table_filter)
{
  --let $mrs_add_db_object_table_filter=
}

--disable_query_log
--disable_result_log

if (!$mrs_sql_id_variable_ob)
{
  --let $mrs_sql_id_variable_ob=@object_id
}

--let $_schema_name=`SELECT name FROM mysql_rest_service_metadata.db_schema WHERE id=$mrs_schema_id_sql_variable`
if ($mrs_sql_id_variable_ob)
{
  if (!`SELECT $mrs_sql_id_variable_ob`)
  {
    eval SET $mrs_sql_id_variable_ob=mysql_rest_service_metadata.get_sequence_id();
    eval INSERT INTO mysql_rest_service_metadata.object(id, db_object_id, name) VALUES($mrs_sql_id_variable_ob, $mrs_sql_id_variable, "$mrs_add_db_object");
  }
}

eval INSERT INTO mysql_rest_service_metadata.object_field
       (id, name, object_id, enabled, position, db_column)
       SELECT mysql_rest_service_metadata.get_sequence_id(), z.cname, $mrs_sql_id_variable_ob, IF(INSTR(z.cname,"hidden"),false,true), z.pos, z.js
       FROM
         (SELECT COLUMN_NAME as cname,
                 ORDINAL_POSITION as pos,
                 JSON_SET("{}",
                 "$.name",COLUMN_NAME,
                 "$.datatype",COLUMN_TYPE, 
                 "$.auto_inc", EXTRA like "%auto_increment%", 
                 "$.not_null", is_NULLABLE <> "YES", 
                 "$.is_primary", COLUMN_KEY like "%PRI%",
                 "$.srid", SRS_ID) as js
                 FROM information_schema.columns where TABLE_SCHEMA="$_schema_name" and TABLE_NAME="$mrs_add_db_object" $mrs_add_db_object_table_filter) as z;

--enable_query_log
--enable_result_log
