select @@global.large_files_support;
@@global.large_files_support
1
select @@session.large_files_support;
ERROR HY000: Variable 'large_files_support' is a GLOBAL variable
show global variables like 'large_files_support';
Variable_name	Value
large_files_support	ON
show session variables like 'large_files_support';
Variable_name	Value
large_files_support	ON
select * from performance_schema.global_variables where variable_name='large_files_support';
VARIABLE_NAME	VARIABLE_VALUE
large_files_support	ON
select * from performance_schema.session_variables where variable_name='large_files_support';
VARIABLE_NAME	VARIABLE_VALUE
large_files_support	ON
set global large_files_support=1;
ERROR HY000: Variable 'large_files_support' is a read only variable
set session large_files_support=1;
ERROR HY000: Variable 'large_files_support' is a read only variable
