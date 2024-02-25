SET @start_global_value = @@global.innodb_flush_neighbors;
SELECT @start_global_value;
@start_global_value
0
select @@global.innodb_flush_neighbors;
@@global.innodb_flush_neighbors
0
select @@session.innodb_flush_neighbors;
ERROR HY000: Variable 'innodb_flush_neighbors' is a GLOBAL variable
show global variables like 'innodb_flush_neighbors';
Variable_name	Value
innodb_flush_neighbors	0
show session variables like 'innodb_flush_neighbors';
Variable_name	Value
innodb_flush_neighbors	0
select * from performance_schema.global_variables where variable_name='innodb_flush_neighbors';
VARIABLE_NAME	VARIABLE_VALUE
innodb_flush_neighbors	0
select * from performance_schema.session_variables where variable_name='innodb_flush_neighbors';
VARIABLE_NAME	VARIABLE_VALUE
innodb_flush_neighbors	0
set global innodb_flush_neighbors=0;
select @@global.innodb_flush_neighbors;
@@global.innodb_flush_neighbors
0
select * from performance_schema.global_variables where variable_name='innodb_flush_neighbors';
VARIABLE_NAME	VARIABLE_VALUE
innodb_flush_neighbors	0
select * from performance_schema.session_variables where variable_name='innodb_flush_neighbors';
VARIABLE_NAME	VARIABLE_VALUE
innodb_flush_neighbors	0
set @@global.innodb_flush_neighbors=TRUE;
select @@global.innodb_flush_neighbors;
@@global.innodb_flush_neighbors
1
select * from performance_schema.global_variables where variable_name='innodb_flush_neighbors';
VARIABLE_NAME	VARIABLE_VALUE
innodb_flush_neighbors	1
select * from performance_schema.session_variables where variable_name='innodb_flush_neighbors';
VARIABLE_NAME	VARIABLE_VALUE
innodb_flush_neighbors	1
set global innodb_flush_neighbors=0;
select @@global.innodb_flush_neighbors;
@@global.innodb_flush_neighbors
0
select * from performance_schema.global_variables where variable_name='innodb_flush_neighbors';
VARIABLE_NAME	VARIABLE_VALUE
innodb_flush_neighbors	0
select * from performance_schema.session_variables where variable_name='innodb_flush_neighbors';
VARIABLE_NAME	VARIABLE_VALUE
innodb_flush_neighbors	0
set @@global.innodb_flush_neighbors=2;
select @@global.innodb_flush_neighbors;
@@global.innodb_flush_neighbors
2
select * from performance_schema.global_variables where variable_name='innodb_flush_neighbors';
VARIABLE_NAME	VARIABLE_VALUE
innodb_flush_neighbors	2
select * from performance_schema.session_variables where variable_name='innodb_flush_neighbors';
VARIABLE_NAME	VARIABLE_VALUE
innodb_flush_neighbors	2
set @@global.innodb_flush_neighbors=DEFAULT;
select @@global.innodb_flush_neighbors;
@@global.innodb_flush_neighbors
0
select * from performance_schema.global_variables where variable_name='innodb_flush_neighbors';
VARIABLE_NAME	VARIABLE_VALUE
innodb_flush_neighbors	0
select * from performance_schema.session_variables where variable_name='innodb_flush_neighbors';
VARIABLE_NAME	VARIABLE_VALUE
innodb_flush_neighbors	0
set session innodb_flush_neighbors=0;
ERROR HY000: Variable 'innodb_flush_neighbors' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.innodb_flush_neighbors=1;
ERROR HY000: Variable 'innodb_flush_neighbors' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_flush_neighbors='OFF';
ERROR 42000: Incorrect argument type to variable 'innodb_flush_neighbors'
set global innodb_flush_neighbors='ON';
ERROR 42000: Incorrect argument type to variable 'innodb_flush_neighbors'
set global innodb_flush_neighbors=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_flush_neighbors'
set global innodb_flush_neighbors=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_flush_neighbors'
set global innodb_flush_neighbors=3;
Warnings:
Warning	1292	Truncated incorrect innodb_flush_neighbors value: '3'
select @@global.innodb_flush_neighbors;
@@global.innodb_flush_neighbors
2
set global innodb_flush_neighbors=-3;
Warnings:
Warning	1292	Truncated incorrect innodb_flush_neighbors value: '-3'
select @@global.innodb_flush_neighbors;
@@global.innodb_flush_neighbors
0
set global innodb_flush_neighbors='AUTO';
ERROR 42000: Incorrect argument type to variable 'innodb_flush_neighbors'
SET @@global.innodb_flush_neighbors = @start_global_value;
SELECT @@global.innodb_flush_neighbors;
@@global.innodb_flush_neighbors
0
