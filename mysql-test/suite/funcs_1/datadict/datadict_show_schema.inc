#### suite/funcs_1/datadict/datadict_show_schema.test

# shows content of tables from INFORMATION_SCHEMA

# usage:

# let $message= <a message for the .result file>;
# let $dbname= <prefix_of_a_cb_name>;
# --source suite/funcs_1/datadict/datadict_show_schema.test

--source include/show_msg.inc

eval select *
  from information_schema.schemata
 where schema_name like '$dbname%';

eval select table_catalog, table_schema, engine
  from information_schema.tables
 where table_schema like '$dbname%';

eval select *
  from information_schema.columns
 where table_schema like '$dbname%';

eval select table_schema, table_name, is_updatable
  from information_schema.views
 where table_schema like '$dbname%';

eval select routine_name, routine_type, security_type, sql_mode
  from information_schema.routines
 where routine_schema like '$dbname%';

eval select table_name, index_schema, index_name, index_type
  from information_schema.statistics
 where table_schema like '$dbname%';

--replace_result $SERVER_NAME <SERVER_NAME>
--sorted_result
eval select *
  from information_schema.user_privileges;
# where grantee="'u_6_401013'@'%'";

eval select *
  from information_schema.column_privileges
 where table_schema like '$dbname%';

eval select *
  from information_schema.table_privileges
 where table_schema like '$dbname%';

eval select *
  from information_schema.key_column_usage
 where table_schema like '$dbname%';

eval SELECT *
  FROM information_schema.triggers
 WHERE trigger_schema LIKE '$dbname%';
