SET @start_global_value = @@global.sync_source_info;
SELECT @start_global_value;
@start_global_value
10000
select @@global.sync_source_info;
@@global.sync_source_info
10000
select @@session.sync_source_info;
ERROR HY000: Variable 'sync_source_info' is a GLOBAL variable
show global variables like 'sync_source_info';
Variable_name	Value
sync_source_info	10000
show session variables like 'sync_source_info';
Variable_name	Value
sync_source_info	10000
select * from performance_schema.global_variables where variable_name='sync_source_info';
VARIABLE_NAME	VARIABLE_VALUE
sync_source_info	10000
select * from performance_schema.session_variables where variable_name='sync_source_info';
VARIABLE_NAME	VARIABLE_VALUE
sync_source_info	10000
set global sync_source_info=1;
select @@global.sync_source_info;
@@global.sync_source_info
1
set session sync_source_info=1;
ERROR HY000: Variable 'sync_source_info' is a GLOBAL variable and should be set with SET GLOBAL
set global sync_source_info=1.1;
ERROR 42000: Incorrect argument type to variable 'sync_source_info'
set global sync_source_info=1e1;
ERROR 42000: Incorrect argument type to variable 'sync_source_info'
set global sync_source_info="foo";
ERROR 42000: Incorrect argument type to variable 'sync_source_info'
set global sync_source_info=0;
select @@global.sync_source_info;
@@global.sync_source_info
0
set global sync_source_info=cast(-1 as unsigned int);
Warnings:
Warning	1292	Truncated incorrect sync_source_info value: '18446744073709551615'
select @@global.sync_source_info;
@@global.sync_source_info
4294967295
SET @@global.sync_source_info = @start_global_value;
SELECT @@global.sync_source_info;
@@global.sync_source_info
10000
