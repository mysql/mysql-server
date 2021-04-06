SET @start_global_value = @@global.innodb_status_output;
SELECT @start_global_value;
@start_global_value
0
Valid values are 'ON' and 'OFF'
select @@global.innodb_status_output in (0, 1);
@@global.innodb_status_output in (0, 1)
1
select @@global.innodb_status_output;
@@global.innodb_status_output
0
select @@session.innodb_status_output;
ERROR HY000: Variable 'innodb_status_output' is a GLOBAL variable
show global variables like 'innodb_status_output';
Variable_name	Value
innodb_status_output	OFF
show session variables like 'innodb_status_output';
Variable_name	Value
innodb_status_output	OFF
select * from performance_schema.global_variables where variable_name='innodb_status_output';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output	OFF
select * from performance_schema.session_variables where variable_name='innodb_status_output';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output	OFF
set global innodb_status_output='OFF';
select @@global.innodb_status_output;
@@global.innodb_status_output
0
select * from performance_schema.global_variables where variable_name='innodb_status_output';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output	OFF
select * from performance_schema.session_variables where variable_name='innodb_status_output';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output	OFF
set @@global.innodb_status_output=1;
select @@global.innodb_status_output;
@@global.innodb_status_output
1
select * from performance_schema.global_variables where variable_name='innodb_status_output';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output	ON
select * from performance_schema.session_variables where variable_name='innodb_status_output';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output	ON
set global innodb_status_output=0;
select @@global.innodb_status_output;
@@global.innodb_status_output
0
select * from performance_schema.global_variables where variable_name='innodb_status_output';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output	OFF
select * from performance_schema.session_variables where variable_name='innodb_status_output';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output	OFF
set @@global.innodb_status_output='ON';
select @@global.innodb_status_output;
@@global.innodb_status_output
1
select * from performance_schema.global_variables where variable_name='innodb_status_output';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output	ON
select * from performance_schema.session_variables where variable_name='innodb_status_output';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output	ON
set session innodb_status_output='OFF';
ERROR HY000: Variable 'innodb_status_output' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.innodb_status_output='ON';
ERROR HY000: Variable 'innodb_status_output' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_status_output=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_status_output'
set global innodb_status_output=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_status_output'
set global innodb_status_output=2;
ERROR 42000: Variable 'innodb_status_output' can't be set to the value of '2'
set global innodb_status_output=-3;
ERROR 42000: Variable 'innodb_status_output' can't be set to the value of '-3'
select @@global.innodb_status_output;
@@global.innodb_status_output
1
select * from performance_schema.global_variables where variable_name='innodb_status_output';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output	ON
select * from performance_schema.session_variables where variable_name='innodb_status_output';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output	ON
set global innodb_status_output=DEFAULT;
select @@global.innodb_status_output;
@@global.innodb_status_output
0
select * from performance_schema.global_variables where variable_name='innodb_status_output';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output	OFF
select * from performance_schema.session_variables where variable_name='innodb_status_output';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output	OFF
set global innodb_status_output='AUTO';
ERROR 42000: Variable 'innodb_status_output' can't be set to the value of 'AUTO'
SET @@global.innodb_status_output = @start_global_value;
SELECT @@global.innodb_status_output;
@@global.innodb_status_output
0
