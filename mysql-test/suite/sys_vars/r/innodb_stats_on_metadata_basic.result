SET @start_global_value = @@global.innodb_stats_on_metadata;
SELECT @start_global_value;
@start_global_value
0
Valid values are 'ON' and 'OFF' 
select @@global.innodb_stats_on_metadata in (0, 1);
@@global.innodb_stats_on_metadata in (0, 1)
1
select @@global.innodb_stats_on_metadata;
@@global.innodb_stats_on_metadata
0
select @@session.innodb_stats_on_metadata;
ERROR HY000: Variable 'innodb_stats_on_metadata' is a GLOBAL variable
show global variables like 'innodb_stats_on_metadata';
Variable_name	Value
innodb_stats_on_metadata	OFF
show session variables like 'innodb_stats_on_metadata';
Variable_name	Value
innodb_stats_on_metadata	OFF
select * from performance_schema.global_variables where variable_name='innodb_stats_on_metadata';
VARIABLE_NAME	VARIABLE_VALUE
innodb_stats_on_metadata	OFF
select * from performance_schema.session_variables where variable_name='innodb_stats_on_metadata';
VARIABLE_NAME	VARIABLE_VALUE
innodb_stats_on_metadata	OFF
set global innodb_stats_on_metadata='OFF';
select @@global.innodb_stats_on_metadata;
@@global.innodb_stats_on_metadata
0
select * from performance_schema.global_variables where variable_name='innodb_stats_on_metadata';
VARIABLE_NAME	VARIABLE_VALUE
innodb_stats_on_metadata	OFF
select * from performance_schema.session_variables where variable_name='innodb_stats_on_metadata';
VARIABLE_NAME	VARIABLE_VALUE
innodb_stats_on_metadata	OFF
set @@global.innodb_stats_on_metadata=1;
select @@global.innodb_stats_on_metadata;
@@global.innodb_stats_on_metadata
1
select * from performance_schema.global_variables where variable_name='innodb_stats_on_metadata';
VARIABLE_NAME	VARIABLE_VALUE
innodb_stats_on_metadata	ON
select * from performance_schema.session_variables where variable_name='innodb_stats_on_metadata';
VARIABLE_NAME	VARIABLE_VALUE
innodb_stats_on_metadata	ON
set global innodb_stats_on_metadata=0;
select @@global.innodb_stats_on_metadata;
@@global.innodb_stats_on_metadata
0
select * from performance_schema.global_variables where variable_name='innodb_stats_on_metadata';
VARIABLE_NAME	VARIABLE_VALUE
innodb_stats_on_metadata	OFF
select * from performance_schema.session_variables where variable_name='innodb_stats_on_metadata';
VARIABLE_NAME	VARIABLE_VALUE
innodb_stats_on_metadata	OFF
set @@global.innodb_stats_on_metadata='ON';
select @@global.innodb_stats_on_metadata;
@@global.innodb_stats_on_metadata
1
select * from performance_schema.global_variables where variable_name='innodb_stats_on_metadata';
VARIABLE_NAME	VARIABLE_VALUE
innodb_stats_on_metadata	ON
select * from performance_schema.session_variables where variable_name='innodb_stats_on_metadata';
VARIABLE_NAME	VARIABLE_VALUE
innodb_stats_on_metadata	ON
set session innodb_stats_on_metadata='OFF';
ERROR HY000: Variable 'innodb_stats_on_metadata' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.innodb_stats_on_metadata='ON';
ERROR HY000: Variable 'innodb_stats_on_metadata' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_stats_on_metadata=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_stats_on_metadata'
set global innodb_stats_on_metadata=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_stats_on_metadata'
set global innodb_stats_on_metadata=2;
ERROR 42000: Variable 'innodb_stats_on_metadata' can't be set to the value of '2'
set global innodb_stats_on_metadata=-3;
ERROR 42000: Variable 'innodb_stats_on_metadata' can't be set to the value of '-3'
select @@global.innodb_stats_on_metadata;
@@global.innodb_stats_on_metadata
1
select * from performance_schema.global_variables where variable_name='innodb_stats_on_metadata';
VARIABLE_NAME	VARIABLE_VALUE
innodb_stats_on_metadata	ON
select * from performance_schema.session_variables where variable_name='innodb_stats_on_metadata';
VARIABLE_NAME	VARIABLE_VALUE
innodb_stats_on_metadata	ON
set global innodb_stats_on_metadata='AUTO';
ERROR 42000: Variable 'innodb_stats_on_metadata' can't be set to the value of 'AUTO'
SET @@global.innodb_stats_on_metadata = @start_global_value;
SELECT @@global.innodb_stats_on_metadata;
@@global.innodb_stats_on_metadata
0
