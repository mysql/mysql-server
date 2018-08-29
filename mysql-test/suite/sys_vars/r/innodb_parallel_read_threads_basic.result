SET @start_global_value = @@global.innodb_parallel_read_threads;
SELECT @start_global_value;
@start_global_value
4
select @@global.innodb_parallel_read_threads in (2, 64);
@@global.innodb_parallel_read_threads in (2, 64)
0
select @@global.innodb_parallel_read_threads;
@@global.innodb_parallel_read_threads
4
select @@session.innodb_parallel_read_threads;
@@session.innodb_parallel_read_threads
4
show global variables like 'innodb_parallel_read_threads';
Variable_name	Value
innodb_parallel_read_threads	4
show session variables like 'innodb_parallel_read_threads';
Variable_name	Value
innodb_parallel_read_threads	4
select * from performance_schema.global_variables where variable_name='innodb_parallel_read_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_parallel_read_threads	4
select * from performance_schema.session_variables where variable_name='innodb_parallel_read_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_parallel_read_threads	4
set global innodb_parallel_read_threads=3;
select @@global.innodb_parallel_read_threads;
@@global.innodb_parallel_read_threads
3
select * from performance_schema.global_variables where variable_name='innodb_parallel_read_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_parallel_read_threads	3
select * from performance_schema.session_variables where variable_name='innodb_parallel_read_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_parallel_read_threads	4
set @@global.innodb_parallel_read_threads=2;
select @@global.innodb_parallel_read_threads;
@@global.innodb_parallel_read_threads
2
select * from performance_schema.global_variables where variable_name='innodb_parallel_read_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_parallel_read_threads	2
select * from performance_schema.session_variables where variable_name='innodb_parallel_read_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_parallel_read_threads	4
set global innodb_parallel_read_threads=0;
Warnings:
Warning	1292	Truncated incorrect innodb_parallel_read_threads value: '0'
select @@global.innodb_parallel_read_threads;
@@global.innodb_parallel_read_threads
1
select * from performance_schema.global_variables where variable_name='innodb_parallel_read_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_parallel_read_threads	1
select * from performance_schema.session_variables where variable_name='innodb_parallel_read_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_parallel_read_threads	4
set @@global.innodb_parallel_read_threads=3;
select @@global.innodb_parallel_read_threads;
@@global.innodb_parallel_read_threads
3
select * from performance_schema.global_variables where variable_name='innodb_parallel_read_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_parallel_read_threads	3
select * from performance_schema.session_variables where variable_name='innodb_parallel_read_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_parallel_read_threads	4
set session innodb_parallel_read_threads=2;
set @@session.innodb_parallel_read_threads=3;
set global innodb_parallel_read_threads=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_parallel_read_threads'
set global innodb_parallel_read_threads=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_parallel_read_threads'
NOTE: The following should fail with ER_WRONG_VALUE_FOR_VAR (BUG#50643)
set global innodb_parallel_read_threads=-3;
Warnings:
Warning	1292	Truncated incorrect innodb_parallel_read_threads value: '-3'
select @@global.innodb_parallel_read_threads;
@@global.innodb_parallel_read_threads
1
select * from performance_schema.global_variables where variable_name='innodb_parallel_read_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_parallel_read_threads	1
select * from performance_schema.session_variables where variable_name='innodb_parallel_read_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_parallel_read_threads	3
set global innodb_parallel_read_threads='AUTO';
ERROR 42000: Incorrect argument type to variable 'innodb_parallel_read_threads'
SET @@global.innodb_parallel_read_threads = @start_global_value;
SELECT @@global.innodb_parallel_read_threads;
@@global.innodb_parallel_read_threads
4
