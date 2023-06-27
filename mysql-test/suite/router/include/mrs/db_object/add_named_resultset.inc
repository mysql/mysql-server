

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@db_object_id
}

if (!$mrs_sql_id_variable_ob)
{
  --let $mrs_sql_id_variable_ob=@object_id
}


if (!$mrs_modify_columns)
{
  --die `mrs_modify_columns` variable is required.
}

if (!$mrs_modify_fields)
{
  --die `mrs_modify_fields` variable is required.
}

if (!$mrs_modify_name)
{
  --die `mrs_modify_name` variable is required.
}

--disable_query_log
--disable_result_log

let $_columns=$mrs_modify_columns;
let $_fileds=$mrs_modify_fields;

if ($mrs_sql_id_variable_ob)
{
  if (!`SELECT $mrs_sql_id_variable_ob`)
  {
    eval SET $mrs_sql_id_variable_ob=mysql_rest_service_metadata.get_sequence_id();
    eval INSERT INTO mysql_rest_service_metadata.object(id, db_object_id, name, kind) VALUES($mrs_sql_id_variable_ob, $mrs_sql_id_variable, "$mrs_modify_name", "RESULT");
  }
}

while($_columns)
{
  #
  # split columns variable names using "," as delimiter
  #
  let $_column_element=`SELECT TRIM(REPLACE(SUBSTRING_INDEX("$_columns", ",", 1),"\n",""))`;
  let $_columns=`SELECT TRIM(SUBSTRING("$_columns", LENGTH("$_column_element") + 2))`;

  #
  # split fileds variable values using "," as delimiter
  #
  let $_fileds_element=`SELECT TRIM(REPLACE(SUBSTRING_INDEX("$_fileds", ",", 1),"\n",""))`;
  let $_fileds=`SELECT TRIM(SUBSTRING("$_fileds", LENGTH("$_fileds_element") + 2))`;

  eval INSERT INTO mysql_rest_service_metadata.object_field
       (id, name, object_id, enabled, position, db_column)
       VALUE(
         mysql_rest_service_metadata.get_sequence_id(),
         "$_fileds_element",
         $mrs_sql_id_variable_ob,1,0,JSON_SET("{}","$.name","$_column_element", "$.datatype", "TEXT"));
}

eval SET $mrs_sql_id_variable_ob=NULL;

--enable_query_log
--enable_result_log

--let $$mrs_modify_columns=
--let $mrs_modify_fields=
--let $mrs_modify_name=


