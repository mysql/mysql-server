select @@global.log_bin_index;
@@global.log_bin_index
REPLACED
select @@session.log_bin_index;
ERROR HY000: Variable 'log_bin_index' is a GLOBAL variable
show global variables like 'log_bin_index';
Variable_name	Value
log_bin_index	REPLACED
show session variables like 'log_bin_index';
Variable_name	Value
log_bin_index	REPLACED
select * from performance_schema.global_variables where variable_name='log_bin_index';
VARIABLE_NAME	VARIABLE_VALUE
log_bin_index	REPLACED
select * from performance_schema.session_variables where variable_name='log_bin_index';
VARIABLE_NAME	VARIABLE_VALUE
log_bin_index	REPLACED
set global log_bin_index=1;
ERROR HY000: Variable 'log_bin_index' is a read only variable
set session log_bin_index=1;
ERROR HY000: Variable 'log_bin_index' is a read only variable
