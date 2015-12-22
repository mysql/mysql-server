select @@global.performance_schema_max_file_classes;
@@global.performance_schema_max_file_classes
123
select @@session.performance_schema_max_file_classes;
ERROR HY000: Variable 'performance_schema_max_file_classes' is a GLOBAL variable
show global variables like 'performance_schema_max_file_classes';
Variable_name	Value
performance_schema_max_file_classes	123
show session variables like 'performance_schema_max_file_classes';
Variable_name	Value
performance_schema_max_file_classes	123
select * from performance_schema.global_variables
where variable_name='performance_schema_max_file_classes';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema_max_file_classes	123
select * from performance_schema.session_variables
where variable_name='performance_schema_max_file_classes';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema_max_file_classes	123
set global performance_schema_max_file_classes=1;
ERROR HY000: Variable 'performance_schema_max_file_classes' is a read only variable
set session performance_schema_max_file_classes=1;
ERROR HY000: Variable 'performance_schema_max_file_classes' is a read only variable
