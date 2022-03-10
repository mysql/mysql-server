SET @start_global_value = @@global.innodb_status_output_locks;
SELECT @start_global_value;
@start_global_value
0
Valid values are 'ON' and 'OFF'
select @@global.innodb_status_output_locks in (0, 1);
@@global.innodb_status_output_locks in (0, 1)
1
select @@global.innodb_status_output_locks;
@@global.innodb_status_output_locks
0
select @@session.innodb_status_output_locks;
ERROR HY000: Variable 'innodb_status_output_locks' is a GLOBAL variable
show global variables like 'innodb_status_output_locks';
Variable_name	Value
innodb_status_output_locks	OFF
show session variables like 'innodb_status_output_locks';
Variable_name	Value
innodb_status_output_locks	OFF
select * from performance_schema.global_variables where variable_name='innodb_status_output_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output_locks	OFF
select * from performance_schema.session_variables where variable_name='innodb_status_output_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output_locks	OFF
set global innodb_status_output_locks='OFF';
select @@global.innodb_status_output_locks;
@@global.innodb_status_output_locks
0
select * from performance_schema.global_variables where variable_name='innodb_status_output_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output_locks	OFF
select * from performance_schema.session_variables where variable_name='innodb_status_output_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output_locks	OFF
set @@global.innodb_status_output_locks=1;
select @@global.innodb_status_output_locks;
@@global.innodb_status_output_locks
1
select * from performance_schema.global_variables where variable_name='innodb_status_output_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output_locks	ON
select * from performance_schema.session_variables where variable_name='innodb_status_output_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output_locks	ON
set global innodb_status_output_locks=0;
select @@global.innodb_status_output_locks;
@@global.innodb_status_output_locks
0
select * from performance_schema.global_variables where variable_name='innodb_status_output_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output_locks	OFF
select * from performance_schema.session_variables where variable_name='innodb_status_output_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output_locks	OFF
set @@global.innodb_status_output_locks='ON';
select @@global.innodb_status_output_locks;
@@global.innodb_status_output_locks
1
select * from performance_schema.global_variables where variable_name='innodb_status_output_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output_locks	ON
select * from performance_schema.session_variables where variable_name='innodb_status_output_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output_locks	ON
set session innodb_status_output_locks='OFF';
ERROR HY000: Variable 'innodb_status_output_locks' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.innodb_status_output_locks='ON';
ERROR HY000: Variable 'innodb_status_output_locks' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_status_output_locks=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_status_output_locks'
set global innodb_status_output_locks=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_status_output_locks'
set global innodb_status_output_locks=2;
ERROR 42000: Variable 'innodb_status_output_locks' can't be set to the value of '2'
set global innodb_status_output_locks=-3;
ERROR 42000: Variable 'innodb_status_output_locks' can't be set to the value of '-3'
select @@global.innodb_status_output_locks;
@@global.innodb_status_output_locks
1
select * from performance_schema.global_variables where variable_name='innodb_status_output_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output_locks	ON
select * from performance_schema.session_variables where variable_name='innodb_status_output_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output_locks	ON
set global innodb_status_output_locks=DEFAULT;
select @@global.innodb_status_output_locks;
@@global.innodb_status_output_locks
0
select * from performance_schema.global_variables where variable_name='innodb_status_output_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output_locks	OFF
select * from performance_schema.session_variables where variable_name='innodb_status_output_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_status_output_locks	OFF
set global innodb_status_output_locks='AUTO';
ERROR 42000: Variable 'innodb_status_output_locks' can't be set to the value of 'AUTO'
SET @@global.innodb_status_output_locks = @start_global_value;
SELECT @@global.innodb_status_output_locks;
@@global.innodb_status_output_locks
0
