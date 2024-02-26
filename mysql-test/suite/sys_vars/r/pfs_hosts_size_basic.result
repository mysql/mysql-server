select @@global.performance_schema_hosts_size;
@@global.performance_schema_hosts_size
123
select @@session.performance_schema_hosts_size;
ERROR HY000: Variable 'performance_schema_hosts_size' is a GLOBAL variable
show global variables like 'performance_schema_hosts_size';
Variable_name	Value
performance_schema_hosts_size	123
show session variables like 'performance_schema_hosts_size';
Variable_name	Value
performance_schema_hosts_size	123
select * from performance_schema.global_variables
where variable_name='performance_schema_hosts_size';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema_hosts_size	123
select * from performance_schema.session_variables
where variable_name='performance_schema_hosts_size';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema_hosts_size	123
set global performance_schema_hosts_size=1;
ERROR HY000: Variable 'performance_schema_hosts_size' is a read only variable
set session performance_schema_hosts_size=1;
ERROR HY000: Variable 'performance_schema_hosts_size' is a read only variable
