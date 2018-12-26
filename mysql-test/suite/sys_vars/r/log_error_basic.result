select @@global.log_error;
@@global.log_error
stderr
select @@session.log_error;
ERROR HY000: Variable 'log_error' is a GLOBAL variable
show global variables like 'log_error';
Variable_name	Value
log_error	stderr
show session variables like 'log_error';
Variable_name	Value
log_error	stderr
select * from performance_schema.global_variables where variable_name='log_error';
VARIABLE_NAME	VARIABLE_VALUE
log_error	stderr
select * from performance_schema.session_variables where variable_name='log_error';
VARIABLE_NAME	VARIABLE_VALUE
log_error	stderr
set global log_error=1;
ERROR HY000: Variable 'log_error' is a read only variable
set session log_error=1;
ERROR HY000: Variable 'log_error' is a read only variable
