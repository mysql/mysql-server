select @@global.port;
@@global.port
MASTER_MYPORT
select @@session.port;
ERROR HY000: Variable 'port' is a GLOBAL variable
show global variables like 'port';
Variable_name	Value
port	MASTER_MYPORT
show session variables like 'port';
Variable_name	Value
port	MASTER_MYPORT
select * from performance_schema.global_variables where variable_name='port';
VARIABLE_NAME	VARIABLE_VALUE
port	MASTER_MYPORT
select * from performance_schema.session_variables where variable_name='port';
VARIABLE_NAME	VARIABLE_VALUE
port	MASTER_MYPORT
set global port=1;
ERROR HY000: Variable 'port' is a read only variable
set session port=1;
ERROR HY000: Variable 'port' is a read only variable
