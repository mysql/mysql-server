select @@global.log_slave_updates;
@@global.log_slave_updates
1
select @@session.log_slave_updates;
ERROR HY000: Variable 'log_slave_updates' is a GLOBAL variable
show global variables like 'log_slave_updates';
Variable_name	Value
log_slave_updates	ON
show session variables like 'log_slave_updates';
Variable_name	Value
log_slave_updates	ON
select * from performance_schema.global_variables where variable_name='log_slave_updates';
VARIABLE_NAME	VARIABLE_VALUE
log_slave_updates	ON
select * from performance_schema.session_variables where variable_name='log_slave_updates';
VARIABLE_NAME	VARIABLE_VALUE
log_slave_updates	ON
set global log_slave_updates=1;
ERROR HY000: Variable 'log_slave_updates' is a read only variable
set session log_slave_updates=1;
ERROR HY000: Variable 'log_slave_updates' is a read only variable
