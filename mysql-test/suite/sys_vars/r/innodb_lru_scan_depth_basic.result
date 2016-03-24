SET @start_global_value = @@global.innodb_lru_scan_depth;
SELECT @start_global_value;
@start_global_value
1024
Valid value 100 or more
select @@global.innodb_lru_scan_depth >= 100;
@@global.innodb_lru_scan_depth >= 100
1
select @@global.innodb_lru_scan_depth;
@@global.innodb_lru_scan_depth
1024
select @@session.innodb_lru_scan_depth;
ERROR HY000: Variable 'innodb_lru_scan_depth' is a GLOBAL variable
show global variables like 'innodb_lru_scan_depth';
Variable_name	Value
innodb_lru_scan_depth	1024
show session variables like 'innodb_lru_scan_depth';
Variable_name	Value
innodb_lru_scan_depth	1024
select * from performance_schema.global_variables where variable_name='innodb_lru_scan_depth';
VARIABLE_NAME	VARIABLE_VALUE
innodb_lru_scan_depth	1024
select * from performance_schema.session_variables where variable_name='innodb_lru_scan_depth';
VARIABLE_NAME	VARIABLE_VALUE
innodb_lru_scan_depth	1024
set global innodb_lru_scan_depth=325;
select @@global.innodb_lru_scan_depth;
@@global.innodb_lru_scan_depth
325
select * from performance_schema.global_variables where variable_name='innodb_lru_scan_depth';
VARIABLE_NAME	VARIABLE_VALUE
innodb_lru_scan_depth	325
select * from performance_schema.session_variables where variable_name='innodb_lru_scan_depth';
VARIABLE_NAME	VARIABLE_VALUE
innodb_lru_scan_depth	325
set session innodb_lru_scan_depth=444;
ERROR HY000: Variable 'innodb_lru_scan_depth' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_lru_scan_depth=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_lru_scan_depth'
set global innodb_lru_scan_depth=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_lru_scan_depth'
set global innodb_lru_scan_depth="foo";
ERROR 42000: Incorrect argument type to variable 'innodb_lru_scan_depth'
set global innodb_lru_scan_depth=7;
Warnings:
Warning	1292	Truncated incorrect innodb_lru_scan_depth value: '7'
select @@global.innodb_lru_scan_depth;
@@global.innodb_lru_scan_depth
100
select * from performance_schema.global_variables where variable_name='innodb_lru_scan_depth';
VARIABLE_NAME	VARIABLE_VALUE
innodb_lru_scan_depth	100
set global innodb_lru_scan_depth=-7;
Warnings:
Warning	1292	Truncated incorrect innodb_lru_scan_depth value: '-7'
select @@global.innodb_lru_scan_depth;
@@global.innodb_lru_scan_depth
100
select * from performance_schema.global_variables where variable_name='innodb_lru_scan_depth';
VARIABLE_NAME	VARIABLE_VALUE
innodb_lru_scan_depth	100
set global innodb_lru_scan_depth=100;
select @@global.innodb_lru_scan_depth;
@@global.innodb_lru_scan_depth
100
SET @@global.innodb_lru_scan_depth = @start_global_value;
SELECT @@global.innodb_lru_scan_depth;
@@global.innodb_lru_scan_depth
1024
