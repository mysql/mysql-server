select @@global.innodb_fill_factor;
@@global.innodb_fill_factor
100
select @@session.innodb_fill_factor;
ERROR HY000: Variable 'innodb_fill_factor' is a GLOBAL variable
show global variables like 'innodb_fill_factor';
Variable_name	Value
innodb_fill_factor	100
show session variables like 'innodb_fill_factor';
Variable_name	Value
innodb_fill_factor	100
select * from performance_schema.global_variables where variable_name='innodb_fill_factor';
VARIABLE_NAME	VARIABLE_VALUE
innodb_fill_factor	100
select * from performance_schema.session_variables where variable_name='innodb_fill_factor';
VARIABLE_NAME	VARIABLE_VALUE
innodb_fill_factor	100
set global innodb_fill_factor=9;
Warnings:
Warning	1292	Truncated incorrect innodb_fill_factor value: '9'
select @@innodb_fill_factor;
@@innodb_fill_factor
10
set global innodb_fill_factor=10;
select @@innodb_fill_factor;
@@innodb_fill_factor
10
set global innodb_fill_factor=75;
select @@innodb_fill_factor;
@@innodb_fill_factor
75
set global innodb_fill_factor=100;
select @@innodb_fill_factor;
@@innodb_fill_factor
100
set global innodb_fill_factor=101;
Warnings:
Warning	1292	Truncated incorrect innodb_fill_factor value: '101'
select @@innodb_fill_factor;
@@innodb_fill_factor
100
set global innodb_fill_factor=100;
