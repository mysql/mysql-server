include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
select @@global.relay_log_basename;
@@global.relay_log_basename
REPLACED
select @@session.relay_log_basename;
ERROR HY000: Variable 'relay_log_basename' is a GLOBAL variable
show global variables like 'relay_log_basename';
Variable_name	Value
relay_log_basename	REPLACED
show session variables like 'relay_log_basename';
Variable_name	Value
relay_log_basename	REPLACED
select * from performance_schema.global_variables where variable_name='relay_log_basename';
VARIABLE_NAME	VARIABLE_VALUE
relay_log_basename	REPLACED
select * from performance_schema.session_variables where variable_name='relay_log_basename';
VARIABLE_NAME	VARIABLE_VALUE
relay_log_basename	REPLACED
set global relay_log_basename=1;
ERROR HY000: Variable 'relay_log_basename' is a read only variable
set session relay_log_basename=1;
ERROR HY000: Variable 'relay_log_basename' is a read only variable
include/rpl_end.inc
