select @@global.slave_skip_errors;
@@global.slave_skip_errors
OFF
select @@session.slave_skip_errors;
ERROR HY000: Variable 'slave_skip_errors' is a GLOBAL variable
show global variables like 'slave_skip_errors';
Variable_name	Value
slave_skip_errors	OFF
show session variables like 'slave_skip_errors';
Variable_name	Value
slave_skip_errors	OFF
select * from performance_schema.global_variables where variable_name='slave_skip_errors';
VARIABLE_NAME	VARIABLE_VALUE
slave_skip_errors	OFF
select * from performance_schema.session_variables where variable_name='slave_skip_errors';
VARIABLE_NAME	VARIABLE_VALUE
slave_skip_errors	OFF
set global slave_skip_errors=1;
ERROR HY000: Variable 'slave_skip_errors' is a read only variable
set session slave_skip_errors=1;
ERROR HY000: Variable 'slave_skip_errors' is a read only variable
