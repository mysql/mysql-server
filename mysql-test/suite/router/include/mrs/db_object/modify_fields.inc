

if (!$mrs_sql_id_variable)
{
  --let $mrs_sql_id_variable=@db_object_id
}

if (!$mrs_modify_columns)
{
  --die `mrs_modify_columns` variable is required.
}

if (!$mrs_modify_fields)
{
  --die `mrs_modify_fields` variable is required.
}

--disable_query_log
--disable_result_log

let $_columns=$mrs_modify_columns;
let $_fileds=$mrs_modify_fields;

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

  eval UPDATE mysql_rest_service_metadata.object_field
     SET name="$_fileds_element"
     WHERE db_column->>"\$.name"="$_column_element";
}

--enable_query_log
--enable_result_log

--let $$mrs_modify_columns=
--let $mrs_modify_fields=


