select @@global.relay_log;
@@global.relay_log
relaylog
select @@session.relay_log;
ERROR HY000: Variable 'relay_log' is a GLOBAL variable
show global variables like 'relay_log';
Variable_name	Value
relay_log	relaylog
show session variables like 'relay_log';
Variable_name	Value
relay_log	relaylog
select * from performance_schema.global_variables where variable_name='relay_log';
VARIABLE_NAME	VARIABLE_VALUE
relay_log	relaylog
select * from performance_schema.session_variables where variable_name='relay_log';
VARIABLE_NAME	VARIABLE_VALUE
relay_log	relaylog
set global relay_log=1;
ERROR HY000: Variable 'relay_log' is a read only variable
set session relay_log=1;
ERROR HY000: Variable 'relay_log' is a read only variable
