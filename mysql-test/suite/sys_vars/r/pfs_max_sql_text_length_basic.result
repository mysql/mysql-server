select @@global.performance_schema_max_sql_text_length;
@@global.performance_schema_max_sql_text_length
123
select @@session.performance_schema_max_sql_text_length;
ERROR HY000: Variable 'performance_schema_max_sql_text_length' is a GLOBAL variable
show global variables like 'performance_schema_max_sql_text_length';
Variable_name	Value
performance_schema_max_sql_text_length	123
show session variables like 'performance_schema_max_sql_text_length';
Variable_name	Value
performance_schema_max_sql_text_length	123
select * from performance_schema.global_variables
where variable_name='performance_schema_max_sql_text_length';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema_max_sql_text_length	123
select * from performance_schema.session_variables
where variable_name='performance_schema_max_sql_text_length';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema_max_sql_text_length	123
set global performance_schema_max_sql_text_length=1;
ERROR HY000: Variable 'performance_schema_max_sql_text_length' is a read only variable
set session performance_schema_max_sql_text_length=1;
ERROR HY000: Variable 'performance_schema_max_sql_text_length' is a read only variable
