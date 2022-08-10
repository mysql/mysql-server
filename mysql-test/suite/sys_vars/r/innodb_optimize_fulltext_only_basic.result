SET @start_global_value = @@global.innodb_optimize_fulltext_only;
SELECT @start_global_value;
@start_global_value
0
Valid values are 'ON' and 'OFF' 
select @@global.innodb_optimize_fulltext_only in (0, 1);
@@global.innodb_optimize_fulltext_only in (0, 1)
1
select @@global.innodb_optimize_fulltext_only;
@@global.innodb_optimize_fulltext_only
0
select @@session.innodb_optimize_fulltext_only;
ERROR HY000: Variable 'innodb_optimize_fulltext_only' is a GLOBAL variable
show global variables like 'innodb_optimize_fulltext_only';
Variable_name	Value
innodb_optimize_fulltext_only	OFF
show session variables like 'innodb_optimize_fulltext_only';
Variable_name	Value
innodb_optimize_fulltext_only	OFF
select * from performance_schema.global_variables where variable_name='innodb_optimize_fulltext_only';
VARIABLE_NAME	VARIABLE_VALUE
innodb_optimize_fulltext_only	OFF
select * from performance_schema.session_variables where variable_name='innodb_optimize_fulltext_only';
VARIABLE_NAME	VARIABLE_VALUE
innodb_optimize_fulltext_only	OFF
set global innodb_optimize_fulltext_only='ON';
select @@global.innodb_optimize_fulltext_only;
@@global.innodb_optimize_fulltext_only
1
select * from performance_schema.global_variables where variable_name='innodb_optimize_fulltext_only';
VARIABLE_NAME	VARIABLE_VALUE
innodb_optimize_fulltext_only	ON
select * from performance_schema.session_variables where variable_name='innodb_optimize_fulltext_only';
VARIABLE_NAME	VARIABLE_VALUE
innodb_optimize_fulltext_only	ON
set @@global.innodb_optimize_fulltext_only=0;
select @@global.innodb_optimize_fulltext_only;
@@global.innodb_optimize_fulltext_only
0
select * from performance_schema.global_variables where variable_name='innodb_optimize_fulltext_only';
VARIABLE_NAME	VARIABLE_VALUE
innodb_optimize_fulltext_only	OFF
select * from performance_schema.session_variables where variable_name='innodb_optimize_fulltext_only';
VARIABLE_NAME	VARIABLE_VALUE
innodb_optimize_fulltext_only	OFF
set global innodb_optimize_fulltext_only=1;
select @@global.innodb_optimize_fulltext_only;
@@global.innodb_optimize_fulltext_only
1
select * from performance_schema.global_variables where variable_name='innodb_optimize_fulltext_only';
VARIABLE_NAME	VARIABLE_VALUE
innodb_optimize_fulltext_only	ON
select * from performance_schema.session_variables where variable_name='innodb_optimize_fulltext_only';
VARIABLE_NAME	VARIABLE_VALUE
innodb_optimize_fulltext_only	ON
set @@global.innodb_optimize_fulltext_only='OFF';
select @@global.innodb_optimize_fulltext_only;
@@global.innodb_optimize_fulltext_only
0
select * from performance_schema.global_variables where variable_name='innodb_optimize_fulltext_only';
VARIABLE_NAME	VARIABLE_VALUE
innodb_optimize_fulltext_only	OFF
select * from performance_schema.session_variables where variable_name='innodb_optimize_fulltext_only';
VARIABLE_NAME	VARIABLE_VALUE
innodb_optimize_fulltext_only	OFF
set session innodb_optimize_fulltext_only='OFF';
ERROR HY000: Variable 'innodb_optimize_fulltext_only' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.innodb_optimize_fulltext_only='ON';
ERROR HY000: Variable 'innodb_optimize_fulltext_only' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_optimize_fulltext_only=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_optimize_fulltext_only'
set global innodb_optimize_fulltext_only=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_optimize_fulltext_only'
set global innodb_optimize_fulltext_only=2;
ERROR 42000: Variable 'innodb_optimize_fulltext_only' can't be set to the value of '2'
set global innodb_optimize_fulltext_only=-3;
ERROR 42000: Variable 'innodb_optimize_fulltext_only' can't be set to the value of '-3'
select @@global.innodb_optimize_fulltext_only;
@@global.innodb_optimize_fulltext_only
0
select * from performance_schema.global_variables where variable_name='innodb_optimize_fulltext_only';
VARIABLE_NAME	VARIABLE_VALUE
innodb_optimize_fulltext_only	OFF
select * from performance_schema.session_variables where variable_name='innodb_optimize_fulltext_only';
VARIABLE_NAME	VARIABLE_VALUE
innodb_optimize_fulltext_only	OFF
set global innodb_optimize_fulltext_only='AUTO';
ERROR 42000: Variable 'innodb_optimize_fulltext_only' can't be set to the value of 'AUTO'
SET @@global.innodb_optimize_fulltext_only = @start_global_value;
SELECT @@global.innodb_optimize_fulltext_only;
@@global.innodb_optimize_fulltext_only
0
