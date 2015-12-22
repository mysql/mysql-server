select @@global.log_bin_basename;
@@global.log_bin_basename
REPLACED
select @@session.log_bin_basename;
ERROR HY000: Variable 'log_bin_basename' is a GLOBAL variable
show global variables like 'log_bin_basename';
Variable_name	Value
log_bin_basename	REPLACED
show session variables like 'log_bin_basename';
Variable_name	Value
log_bin_basename	REPLACED
select * from performance_schema.global_variables where variable_name='log_bin_basename';
VARIABLE_NAME	VARIABLE_VALUE
log_bin_basename	REPLACED
select * from performance_schema.session_variables where variable_name='log_bin_basename';
VARIABLE_NAME	VARIABLE_VALUE
log_bin_basename	REPLACED
set global log_bin_basename=1;
ERROR HY000: Variable 'log_bin_basename' is a read only variable
set session log_bin_basename=1;
ERROR HY000: Variable 'log_bin_basename' is a read only variable
