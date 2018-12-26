select @@global.performance_schema;
@@global.performance_schema
1
select @@session.performance_schema;
ERROR HY000: Variable 'performance_schema' is a GLOBAL variable
show global variables like 'performance_schema';
Variable_name	Value
performance_schema	ON
show session variables like 'performance_schema';
Variable_name	Value
performance_schema	ON
select * from performance_schema.global_variables
where variable_name='performance_schema';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema	ON
select * from performance_schema.session_variables
where variable_name='performance_schema';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema	ON
set global performance_schema=1;
ERROR HY000: Variable 'performance_schema' is a read only variable
set session performance_schema=1;
ERROR HY000: Variable 'performance_schema' is a read only variable
