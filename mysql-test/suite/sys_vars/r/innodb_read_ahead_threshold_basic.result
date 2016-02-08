SET @start_global_value = @@global.innodb_read_ahead_threshold;
SELECT @start_global_value;
@start_global_value
56
Valid values are between 0 and 64
select @@global.innodb_read_ahead_threshold between 0 and 64;
@@global.innodb_read_ahead_threshold between 0 and 64
1
select @@global.innodb_read_ahead_threshold;
@@global.innodb_read_ahead_threshold
56
select @@session.innodb_read_ahead_threshold;
ERROR HY000: Variable 'innodb_read_ahead_threshold' is a GLOBAL variable
show global variables like 'innodb_read_ahead_threshold';
Variable_name	Value
innodb_read_ahead_threshold	56
show session variables like 'innodb_read_ahead_threshold';
Variable_name	Value
innodb_read_ahead_threshold	56
select * from performance_schema.global_variables where variable_name='innodb_read_ahead_threshold';
VARIABLE_NAME	VARIABLE_VALUE
innodb_read_ahead_threshold	56
select * from performance_schema.session_variables where variable_name='innodb_read_ahead_threshold';
VARIABLE_NAME	VARIABLE_VALUE
innodb_read_ahead_threshold	56
set global innodb_read_ahead_threshold=10;
select @@global.innodb_read_ahead_threshold;
@@global.innodb_read_ahead_threshold
10
select * from performance_schema.global_variables where variable_name='innodb_read_ahead_threshold';
VARIABLE_NAME	VARIABLE_VALUE
innodb_read_ahead_threshold	10
select * from performance_schema.session_variables where variable_name='innodb_read_ahead_threshold';
VARIABLE_NAME	VARIABLE_VALUE
innodb_read_ahead_threshold	10
set session innodb_read_ahead_threshold=1;
ERROR HY000: Variable 'innodb_read_ahead_threshold' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_read_ahead_threshold=DEFAULT;
select @@global.innodb_read_ahead_threshold;
@@global.innodb_read_ahead_threshold
56
set global innodb_read_ahead_threshold=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_read_ahead_threshold'
set global innodb_read_ahead_threshold=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_read_ahead_threshold'
set global innodb_read_ahead_threshold="foo";
ERROR 42000: Incorrect argument type to variable 'innodb_read_ahead_threshold'
set global innodb_read_ahead_threshold=' ';
ERROR 42000: Incorrect argument type to variable 'innodb_read_ahead_threshold'
select @@global.innodb_read_ahead_threshold;
@@global.innodb_read_ahead_threshold
56
set global innodb_read_ahead_threshold=" ";
ERROR 42000: Incorrect argument type to variable 'innodb_read_ahead_threshold'
select @@global.innodb_read_ahead_threshold;
@@global.innodb_read_ahead_threshold
56
set global innodb_read_ahead_threshold=-7;
Warnings:
Warning	1292	Truncated incorrect innodb_read_ahead_threshold value: '-7'
select @@global.innodb_read_ahead_threshold;
@@global.innodb_read_ahead_threshold
0
select * from performance_schema.global_variables where variable_name='innodb_read_ahead_threshold';
VARIABLE_NAME	VARIABLE_VALUE
innodb_read_ahead_threshold	0
set global innodb_read_ahead_threshold=96;
Warnings:
Warning	1292	Truncated incorrect innodb_read_ahead_threshold value: '96'
select @@global.innodb_read_ahead_threshold;
@@global.innodb_read_ahead_threshold
64
select * from performance_schema.global_variables where variable_name='innodb_read_ahead_threshold';
VARIABLE_NAME	VARIABLE_VALUE
innodb_read_ahead_threshold	64
set global innodb_read_ahead_threshold=0;
select @@global.innodb_read_ahead_threshold;
@@global.innodb_read_ahead_threshold
0
set global innodb_read_ahead_threshold=64;
select @@global.innodb_read_ahead_threshold;
@@global.innodb_read_ahead_threshold
64
SET @@global.innodb_read_ahead_threshold = @start_global_value;
SELECT @@global.innodb_read_ahead_threshold;
@@global.innodb_read_ahead_threshold
56
