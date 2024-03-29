SET @start_global_value = @@global.sync_relay_log;
SELECT @start_global_value;
@start_global_value
10000
select @@global.sync_relay_log;
@@global.sync_relay_log
10000
select @@session.sync_relay_log;
ERROR HY000: Variable 'sync_relay_log' is a GLOBAL variable
show global variables like 'sync_relay_log';
Variable_name	Value
sync_relay_log	10000
show session variables like 'sync_relay_log';
Variable_name	Value
sync_relay_log	10000
select * from performance_schema.global_variables where variable_name='sync_relay_log';
VARIABLE_NAME	VARIABLE_VALUE
sync_relay_log	10000
select * from performance_schema.session_variables where variable_name='sync_relay_log';
VARIABLE_NAME	VARIABLE_VALUE
sync_relay_log	10000
set global sync_relay_log=1;
select @@global.sync_relay_log;
@@global.sync_relay_log
1
set session sync_relay_log=1;
ERROR HY000: Variable 'sync_relay_log' is a GLOBAL variable and should be set with SET GLOBAL
set global sync_relay_log=1.1;
ERROR 42000: Incorrect argument type to variable 'sync_relay_log'
set global sync_relay_log=1e1;
ERROR 42000: Incorrect argument type to variable 'sync_relay_log'
set global sync_relay_log="foo";
ERROR 42000: Incorrect argument type to variable 'sync_relay_log'
set global sync_relay_log=0;
select @@global.sync_relay_log;
@@global.sync_relay_log
0
set global sync_relay_log=cast(-1 as unsigned int);
Warnings:
Warning	1292	Truncated incorrect sync_relay_log value: '18446744073709551615'
select @@global.sync_relay_log;
@@global.sync_relay_log
4294967295
SET @@global.sync_relay_log = @start_global_value;
SELECT @@global.sync_relay_log;
@@global.sync_relay_log
10000
