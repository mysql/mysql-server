select @@global.back_log;
@@global.back_log
151
select @@session.back_log;
ERROR HY000: Variable 'back_log' is a GLOBAL variable
show global variables like 'back_log';
Variable_name	Value
back_log	151
show session variables like 'back_log';
Variable_name	Value
back_log	151
select * from performance_schema.global_variables where variable_name='back_log';
VARIABLE_NAME	VARIABLE_VALUE
back_log	151
select * from performance_schema.session_variables where variable_name='back_log';
VARIABLE_NAME	VARIABLE_VALUE
back_log	151
set global back_log=1;
ERROR HY000: Variable 'back_log' is a read only variable
set session back_log=1;
ERROR HY000: Variable 'back_log' is a read only variable
