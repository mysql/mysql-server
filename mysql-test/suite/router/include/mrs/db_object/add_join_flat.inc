

if (!$mrs_sql_id_variable_db_object)
{
  --let $mrs_sql_id_variable_db_object=@db_object_id
}

if (!$mrs_sql_id_variable_object)
{
  --let $mrs_sql_id_variable_object=@object_id
}

if (!$mrs_sql_id_variable_object_ref)
{
  --let $mrs_sql_id_variable_object_ref=@object_ref
}

if (!$mrs_add_db_object_join_schema)
{
  --die 'mrs_add_db_object_join_schema' variable is required.
}

if (!$mrs_add_db_object_join_table)
{
  --die 'mrs_add_db_object_join_table' variable is required.
}

if (!$mrs_add_db_object_join_on_base)
{
  --die 'mrs_add_db_object_join_on' variable is required.
}

if (!$mrs_add_db_object_join_on_other)
{
  --die 'mrs_add_db_object_join_on' variable is required.
}

if (!$mrs_add_db_object_op)
{
  --let $mrs_add_db_object_op=CREATE,READ,UPDATE,DELETE
}

if ($mrs_add_db_object_join_columns)
{
  --let $mrs_add_db_object_join_columns= AND FIND_IN_SET(COLUMN_NAME, "$mrs_add_db_object_join_columns") > 0
}


--disable_query_log
--disable_result_log
eval SET $mrs_sql_id_variable_object=(
      SELECT object.id FROM mysql_rest_service_metadata.object
        WHERE object.db_object_id=$mrs_sql_id_variable_db_object);
eval SET $mrs_sql_id_variable_object_ref=mysql_rest_service_metadata.get_sequence_id();

--let $_columns1=$mrs_add_db_object_join_on_base
--let $_columns2=$mrs_add_db_object_join_on_other

SET @_fields="[]";
while($_columns1)
{
  #
  # split columns variable names using "," as delimiter
  #
  let $_column1_element=`SELECT TRIM(REPLACE(SUBSTRING_INDEX("$_columns1", ",", 1),"\n",""))`;
  let $_columns1=`SELECT TRIM(SUBSTRING("$_columns1", LENGTH("$_column1_element") + 2))`;

  #
  # split columns variable names using "," as delimiter
  #
  let $_column2_element=`SELECT TRIM(REPLACE(SUBSTRING_INDEX("$_columns2", ",", 1),"\n",""))`;
  let $_columns2=`SELECT TRIM(SUBSTRING("$_columns2", LENGTH("$_column2_element") + 2))`;

  eval SET @_fields=JSON_ARRAY_APPEND(@_fields, "$", JSON_OBJECT("base","$_column1_element","ref","$_column2_element"));
}

--let $_mrs_do_default_object_ref=1
if ($mrs_schema_version == 2)
{
  --let $_mrs_do_default_object_ref=0
  eval  INSERT INTO mysql_rest_service_metadata.object_reference(
               id, reference_mapping, unnest, crud_operations)
      VALUES($mrs_sql_id_variable_object_ref, JSON_OBJECT(
           "referenced_schema","$mrs_add_db_object_join_schema",
           "referenced_table", "$mrs_add_db_object_join_table",
           "to_many", False,
           "column_mapping",cast(@_fields as JSON)), True,
             "$mrs_add_db_object_op");
}

if ($_mrs_do_default_object_ref)
{
  eval  INSERT INTO mysql_rest_service_metadata.object_reference(
               id, reference_mapping, unnest)
      VALUES($mrs_sql_id_variable_object_ref, JSON_OBJECT(
           "referenced_schema","$mrs_add_db_object_join_schema",
           "referenced_table", "$mrs_add_db_object_join_table",
           "to_many", False,
           "column_mapping",cast(@_fields as JSON)), True);
}

--echo # Registred OBJECT_REF at $mrs_add_db_object_join_schema.$mrs_add_db_object_join_table

if (!$mrs_add_db_object_do_not_auto_register_columns)
{
    if (!$mrs_sql_id_variable_ob)
    {
      --let $mrs_sql_id_variable_ob=@object_id
    }
    eval INSERT INTO mysql_rest_service_metadata.object_field(
           id, parent_reference_id, name, object_id, enabled, position, db_column)
           SELECT
            mysql_rest_service_metadata.get_sequence_id(), IF(INSTR(z.cname,"hidden"),NULL,$mrs_sql_id_variable_object_ref),
            z.cname, $mrs_sql_id_variable_ob, IF(INSTR(z.cname,"hidden"),false,true), z.pos, z.js
           FROM (SELECT COLUMN_NAME as cname, ORDINAL_POSITION as pos,
                        JSON_SET("{}","$.name",column_name,"$.datatype",data_type,
                                 "$.auto_inc", EXTRA like "%auto_increment%", "$.not_null", is_NULLABLE <> "YES",
                                 "$.is_primary", COLUMN_KEY like "%PRI%") as js
                        FROM information_schema.columns
                        WHERE
                           TABLE_SCHEMA="$mrs_add_db_object_join_schema"
                           AND TABLE_NAME="$mrs_add_db_object_join_table"
                            $mrs_add_db_object_join_columns) as z;

    let $_id=`SELECT max(position) + 1 FROM mysql_rest_service_metadata.object_field
                     WHERE object_id=$mrs_sql_id_variable_object`;
    eval INSERT INTO mysql_rest_service_metadata.object_field(
               id, represents_reference_id, name, object_id, enabled, position, db_column)
               VALUES(
                  mysql_rest_service_metadata.get_sequence_id(),
                  $mrs_sql_id_variable_object_ref,
                  "something",
                  $mrs_sql_id_variable_object, 1,
                  $_id,
                  null);
}


--enable_query_log
--enable_result_log

--let $mrs_sql_id_variable_db_object=
--let $mrs_sql_id_variable_object=
--let $mrs_sql_id_variable_ob=
--let $mrs_sql_id_variable_object_ref=
--let $mrs_add_db_object_join_schema=
--let $mrs_add_db_object_join_table=
--let $mrs_add_db_object_join_on=
--let $mrs_add_db_object_op=
--let $mrs_add_db_object_join_columns=

