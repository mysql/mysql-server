select @@global.relay_log_space_limit;
@@global.relay_log_space_limit
0
select @@session.relay_log_space_limit;
ERROR HY000: Variable 'relay_log_space_limit' is a GLOBAL variable
show global variables like 'relay_log_space_limit';
Variable_name	Value
relay_log_space_limit	0
show session variables like 'relay_log_space_limit';
Variable_name	Value
relay_log_space_limit	0
select * from performance_schema.global_variables where variable_name='relay_log_space_limit';
VARIABLE_NAME	VARIABLE_VALUE
relay_log_space_limit	0
select * from performance_schema.session_variables where variable_name='relay_log_space_limit';
VARIABLE_NAME	VARIABLE_VALUE
relay_log_space_limit	0
set global relay_log_space_limit=1;
ERROR HY000: Variable 'relay_log_space_limit' is a read only variable
set session relay_log_space_limit=1;
ERROR HY000: Variable 'relay_log_space_limit' is a read only variable
