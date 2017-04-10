select @@global.performance_schema_max_table_lock_stat;
@@global.performance_schema_max_table_lock_stat
123
select @@session.performance_schema_max_table_lock_stat;
ERROR HY000: Variable 'performance_schema_max_table_lock_stat' is a GLOBAL variable
show global variables like 'performance_schema_max_table_lock_stat';
Variable_name	Value
performance_schema_max_table_lock_stat	123
show session variables like 'performance_schema_max_table_lock_stat';
Variable_name	Value
performance_schema_max_table_lock_stat	123
select * from performance_schema.global_variables
where variable_name='performance_schema_max_table_lock_stat';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema_max_table_lock_stat	123
select * from performance_schema.session_variables
where variable_name='performance_schema_max_table_lock_stat';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema_max_table_lock_stat	123
set global performance_schema_max_table_lock_stat=1;
ERROR HY000: Variable 'performance_schema_max_table_lock_stat' is a read only variable
set session performance_schema_max_table_lock_stat=1;
ERROR HY000: Variable 'performance_schema_max_table_lock_stat' is a read only variable
