select @@global.relay_log_recovery;
@@global.relay_log_recovery
0
select @@session.relay_log_recovery;
ERROR HY000: Variable 'relay_log_recovery' is a GLOBAL variable
show global variables like 'relay_log_recovery';
Variable_name	Value
relay_log_recovery	OFF
show session variables like 'relay_log_recovery';
Variable_name	Value
relay_log_recovery	OFF
select * from performance_schema.global_variables where variable_name='relay_log_recovery';
VARIABLE_NAME	VARIABLE_VALUE
relay_log_recovery	OFF
select * from performance_schema.session_variables where variable_name='relay_log_recovery';
VARIABLE_NAME	VARIABLE_VALUE
relay_log_recovery	OFF
set global relay_log_recovery=1;
ERROR HY000: Variable 'relay_log_recovery' is a read only variable
select @@global.relay_log_recovery;
@@global.relay_log_recovery
0
select * from performance_schema.global_variables where variable_name='relay_log_recovery';
VARIABLE_NAME	VARIABLE_VALUE
relay_log_recovery	OFF
select * from performance_schema.session_variables where variable_name='relay_log_recovery';
VARIABLE_NAME	VARIABLE_VALUE
relay_log_recovery	OFF
