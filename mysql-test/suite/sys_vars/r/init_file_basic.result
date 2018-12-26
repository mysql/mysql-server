select @@global.init_file;
@@global.init_file
NULL
select @@session.init_file;
ERROR HY000: Variable 'init_file' is a GLOBAL variable
show global variables like 'init_file';
Variable_name	Value
init_file	
show session variables like 'init_file';
Variable_name	Value
init_file	
select * from performance_schema.global_variables where variable_name='init_file';
VARIABLE_NAME	VARIABLE_VALUE
init_file	
select * from performance_schema.session_variables where variable_name='init_file';
VARIABLE_NAME	VARIABLE_VALUE
init_file	
set global init_file=1;
ERROR HY000: Variable 'init_file' is a read only variable
set session init_file=1;
ERROR HY000: Variable 'init_file' is a read only variable
