select @@global.log_bin;
@@global.log_bin
1
select @@session.log_bin;
ERROR HY000: Variable 'log_bin' is a GLOBAL variable
show global variables like 'log_bin';
Variable_name	Value
log_bin	ON
show session variables like 'log_bin';
Variable_name	Value
log_bin	ON
select * from performance_schema.global_variables where variable_name='log_bin';
VARIABLE_NAME	VARIABLE_VALUE
log_bin	ON
select * from performance_schema.session_variables where variable_name='log_bin';
VARIABLE_NAME	VARIABLE_VALUE
log_bin	ON
set global log_bin=1;
ERROR HY000: Variable 'log_bin' is a read only variable
set session log_bin=1;
ERROR HY000: Variable 'log_bin' is a read only variable
