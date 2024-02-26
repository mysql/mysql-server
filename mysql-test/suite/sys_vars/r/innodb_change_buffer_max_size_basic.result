SET @start_global_value = @@global.innodb_change_buffer_max_size;
SELECT @start_global_value;
@start_global_value
25
Valid values are between 0 and 50
select @@global.innodb_change_buffer_max_size between 0 and 50;
@@global.innodb_change_buffer_max_size between 0 and 50
1
select @@global.innodb_change_buffer_max_size;
@@global.innodb_change_buffer_max_size
25
select @@session.innodb_change_buffer_max_size;
ERROR HY000: Variable 'innodb_change_buffer_max_size' is a GLOBAL variable
show global variables like 'innodb_change_buffer_max_size';
Variable_name	Value
innodb_change_buffer_max_size	25
show session variables like 'innodb_change_buffer_max_size';
Variable_name	Value
innodb_change_buffer_max_size	25
select * from performance_schema.global_variables where variable_name='innodb_change_buffer_max_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_change_buffer_max_size	25
select * from performance_schema.session_variables where variable_name='innodb_change_buffer_max_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_change_buffer_max_size	25
set global innodb_change_buffer_max_size=10;
select @@global.innodb_change_buffer_max_size;
@@global.innodb_change_buffer_max_size
10
select * from performance_schema.global_variables where variable_name='innodb_change_buffer_max_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_change_buffer_max_size	10
select * from performance_schema.session_variables where variable_name='innodb_change_buffer_max_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_change_buffer_max_size	10
set session innodb_change_buffer_max_size=1;
ERROR HY000: Variable 'innodb_change_buffer_max_size' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_change_buffer_max_size=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_change_buffer_max_size'
set global innodb_change_buffer_max_size=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_change_buffer_max_size'
set global innodb_change_buffer_max_size="foo";
ERROR 42000: Incorrect argument type to variable 'innodb_change_buffer_max_size'
set global innodb_change_buffer_max_size=-7;
Warnings:
Warning	1292	Truncated incorrect innodb_change_buffer_max_size value: '-7'
select @@global.innodb_change_buffer_max_size;
@@global.innodb_change_buffer_max_size
0
select * from performance_schema.global_variables where variable_name='innodb_change_buffer_max_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_change_buffer_max_size	0
set global innodb_change_buffer_max_size=56;
Warnings:
Warning	1292	Truncated incorrect innodb_change_buffer_max_size value: '56'
select @@global.innodb_change_buffer_max_size;
@@global.innodb_change_buffer_max_size
50
select * from performance_schema.global_variables where variable_name='innodb_change_buffer_max_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_change_buffer_max_size	50
set global innodb_change_buffer_max_size=0;
select @@global.innodb_change_buffer_max_size;
@@global.innodb_change_buffer_max_size
0
set global innodb_change_buffer_max_size=50;
select @@global.innodb_change_buffer_max_size;
@@global.innodb_change_buffer_max_size
50
set global innodb_change_buffer_max_size=DEFAULT;
select @@global.innodb_change_buffer_max_size;
@@global.innodb_change_buffer_max_size
25
SET @@global.innodb_change_buffer_max_size = @start_global_value;
SELECT @@global.innodb_change_buffer_max_size;
@@global.innodb_change_buffer_max_size
25
