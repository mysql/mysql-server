SET @start_global_value = @@global.innodb_idle_flush_pct;
SELECT @start_global_value;
@start_global_value
100
Valid values are between 0 and 100
select @@global.innodb_idle_flush_pct between 0 and 100;
@@global.innodb_idle_flush_pct between 0 and 100
1
select @@global.innodb_idle_flush_pct;
@@global.innodb_idle_flush_pct
100
select @@session.innodb_idle_flush_pct;
ERROR HY000: Variable 'innodb_idle_flush_pct' is a GLOBAL variable
show global variables like 'innodb_idle_flush_pct';
Variable_name	Value
innodb_idle_flush_pct	100
show session variables like 'innodb_idle_flush_pct';
Variable_name	Value
innodb_idle_flush_pct	100
select * from performance_schema.global_variables where variable_name='innodb_idle_flush_pct';
VARIABLE_NAME	VARIABLE_VALUE
innodb_idle_flush_pct	100
select * from performance_schema.session_variables where variable_name='innodb_idle_flush_pct';
VARIABLE_NAME	VARIABLE_VALUE
innodb_idle_flush_pct	100
set global innodb_idle_flush_pct=10;
select @@global.innodb_idle_flush_pct;
@@global.innodb_idle_flush_pct
10
select * from performance_schema.global_variables where variable_name='innodb_idle_flush_pct';
VARIABLE_NAME	VARIABLE_VALUE
innodb_idle_flush_pct	10
select * from performance_schema.session_variables where variable_name='innodb_idle_flush_pct';
VARIABLE_NAME	VARIABLE_VALUE
innodb_idle_flush_pct	10
set session innodb_idle_flush_pct=1;
ERROR HY000: Variable 'innodb_idle_flush_pct' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_idle_flush_pct=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_idle_flush_pct'
set global innodb_idle_flush_pct=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_idle_flush_pct'
set global innodb_idle_flush_pct="bar";
ERROR 42000: Incorrect argument type to variable 'innodb_idle_flush_pct'
set global innodb_idle_flush_pct=ON;
ERROR 42000: Incorrect argument type to variable 'innodb_idle_flush_pct'
set global innodb_idle_flush_pct=OFF;
ERROR 42000: Incorrect argument type to variable 'innodb_idle_flush_pct'
set global innodb_idle_flush_pct=-7;
Warnings:
Warning	1292	Truncated incorrect innodb_idle_flush_pct value: '-7'
select @@global.innodb_idle_flush_pct;
@@global.innodb_idle_flush_pct
0
select * from performance_schema.global_variables where variable_name='innodb_idle_flush_pct';
VARIABLE_NAME	VARIABLE_VALUE
innodb_idle_flush_pct	0
set global innodb_idle_flush_pct=101;
Warnings:
Warning	1292	Truncated incorrect innodb_idle_flush_pct value: '101'
select @@global.innodb_idle_flush_pct;
@@global.innodb_idle_flush_pct
100
select * from performance_schema.global_variables where variable_name='innodb_idle_flush_pct';
VARIABLE_NAME	VARIABLE_VALUE
innodb_idle_flush_pct	100
set global innodb_idle_flush_pct=0;
select @@global.innodb_idle_flush_pct;
@@global.innodb_idle_flush_pct
0
set global innodb_idle_flush_pct=100;
select @@global.innodb_idle_flush_pct;
@@global.innodb_idle_flush_pct
100
set global innodb_idle_flush_pct=DEFAULT;
select @@global.innodb_idle_flush_pct;
@@global.innodb_idle_flush_pct
100
SET @@global.innodb_idle_flush_pct = @start_global_value;
SELECT @@global.innodb_idle_flush_pct;
@@global.innodb_idle_flush_pct
100
