SET DEBUG='+d,skip_dd_table_access_check';

--echo #
--echo # =======================================================================
--echo # Show mysql dd structure of $db_name.$table_name
--echo # =======================================================================
--echo #

# check engine first
--let $engine=`SELECT t.engine as engine FROM mysql.tables t, mysql.schemata s WHERE s.name='$db_name' and s.id = t.schema_id and t.name = '$table_name'`
--let $assert_text= table is not duckdb, which is $engine
--let $assert_cond= "$engine" = "DUCKDB"
--source include/assert.inc

# check partition info
--let $partition_expression=`SELECT t.partition_expression as partition_expression FROM mysql.tables t, mysql.schemata s WHERE s.name='$db_name' and s.id = t.schema_id and t.name = '$table_name'`
--let $assert_text= duckdb not support partition_expression, which is $partition_expression
--let $assert_cond= "$partition_expression" = ""
--source include/assert.inc

--echo # Show colunmn info of $table_name
--let $sql=SELECT c.name, c.type, c.is_nullable, c.is_zerofill, c.is_unsigned, c.datetime_precision, c.comment FROM mysql.columns c, mysql.tables t, mysql.schemata s WHERE s.name='$db_name' and s.id = t.schema_id and c.table_id = t.id and t.name = '$table_name' ORDER BY t.schema_id, t.name, c.name
--eval $sql

# duckdb do not support auto increment
--let $auto_increment_flag=`SELECT c.is_auto_increment as is_auto_increment FROM mysql.columns c, mysql.tables t, mysql.schemata s WHERE s.name='$db_name' and s.id = t.schema_id and c.table_id = t.id and t.name = '$table_name' ORDER BY t.schema_id, t.name, c.name`
--let $assert_text= duckdb do not support auto increment
--let $assert_cond= "$auto_increment_flag" = "0"
--source include/assert.inc


# duckdb do not support virtual column
--let $is_virtual=`SELECT group_concat(distinct(c.is_virtual)) as is_virtual FROM mysql.columns c, mysql.tables t, mysql.schemata s WHERE s.name='$db_name' and s.id = t.schema_id and c.table_id = t.id and t.name = '$table_name'`
--let $assert_text= duckdb do not support virtual column, but is_virtual is $is_virtual
--let $assert_cond= "$is_virtual" = "0"
--source include/assert.inc

--let $generation_expression=`SELECT group_concat(distinct(c.generation_expression)) as generation_expression FROM mysql.columns c, mysql.tables t, mysql.schemata s WHERE s.name='$db_name' and s.id = t.schema_id and c.table_id = t.id and t.name = '$table_name'`
--let $assert_text= duckdb do not support virtual column, but generation_expression is $generation_expression
--let $assert_cond= "$generation_expression" = ""
--source include/assert.inc

# TODO: gipk need hidden
# # duckdb do not support hidden column
# --let $assert_text= duckdb do not support hidden, but it is $hidden
# --let $hidden=`SELECT group_concat(distinct c.hidden) as hidden FROM mysql.columns c, mysql.tables t, mysql.schemata s WHERE s.name='$db_name' and s.id = t.schema_id and c.table_id = t.id and t.name = '$table_name'`
# --let $assert_cond= "$hidden" = "Visible"
# --source include/assert.inc

# duckdb do not support se_private_data
--let $se_private_data=`SELECT group_concat(distinct c.se_private_data) as se_private_data FROM mysql.columns c, mysql.tables t, mysql.schemata s WHERE s.name='$db_name' and s.id = t.schema_id and c.table_id = t.id and t.name = '$table_name'`
--let $assert_text= duckdb do not support se_private_data, but it is $se_private_data
--let $assert_cond= "$se_private_data" = ""
--source include/assert.inc

# TODO: create table t_tinyblob (a tinyblob, b tinyblob, c tinyblob) shows is_explicit_collation=1, I don't know why yet.
# # duckdb do not support explicit_collation
# --let $is_explicit_collation=`SELECT group_concat(distinct c.is_explicit_collation) as is_explicit_collation FROM mysql.columns c, mysql.tables t, mysql.schemata s WHERE s.name='$db_name' and s.id = t.schema_id and c.table_id = t.id and t.name = '$table_name'`
# --let $assert_text= duckdb do not support is_explicit_collation, but it is $is_explicit_collation
# --let $assert_cond= "$is_explicit_collation" = "0"
# --source include/assert.inc

SET DEBUG='-d,skip_dd_table_access_check';
