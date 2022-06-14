SET @start_global_value = @@global.innodb_log_flush_now;
SELECT @start_global_value;
@start_global_value
0
select @@global.innodb_log_flush_now in (0, 1);
@@global.innodb_log_flush_now in (0, 1)
1
select @@global.innodb_log_flush_now;
@@global.innodb_log_flush_now
0
select @@session.innodb_log_flush_now;
ERROR HY000: Variable 'innodb_log_flush_now' is a GLOBAL variable
show global variables like 'innodb_log_flush_now';
Variable_name	Value
innodb_log_flush_now	OFF
show session variables like 'innodb_log_flush_now';
Variable_name	Value
innodb_log_flush_now	OFF
select * from performance_schema.global_variables where variable_name='innodb_log_flush_now';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_flush_now	OFF
select * from performance_schema.session_variables where variable_name='innodb_log_flush_now';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_flush_now	OFF
set global innodb_log_flush_now=1;
select @@global.innodb_log_flush_now;
@@global.innodb_log_flush_now
0
select * from performance_schema.global_variables where variable_name='innodb_log_flush_now';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_flush_now	OFF
select * from performance_schema.session_variables where variable_name='innodb_log_flush_now';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_flush_now	OFF
set @@global.innodb_log_flush_now=0;
select @@global.innodb_log_flush_now;
@@global.innodb_log_flush_now
0
select * from performance_schema.global_variables where variable_name='innodb_log_flush_now';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_flush_now	OFF
select * from performance_schema.session_variables where variable_name='innodb_log_flush_now';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_flush_now	OFF
set global innodb_log_flush_now=ON;
select @@global.innodb_log_flush_now;
@@global.innodb_log_flush_now
0
select * from performance_schema.global_variables where variable_name='innodb_log_flush_now';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_flush_now	OFF
select * from performance_schema.session_variables where variable_name='innodb_log_flush_now';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_flush_now	OFF
set global innodb_log_flush_now=OFF;
select @@global.innodb_log_flush_now;
@@global.innodb_log_flush_now
0
select * from performance_schema.global_variables where variable_name='innodb_log_flush_now';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_flush_now	OFF
select * from performance_schema.session_variables where variable_name='innodb_log_flush_now';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_flush_now	OFF
set session innodb_log_flush_now='some';
ERROR HY000: Variable 'innodb_log_flush_now' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.innodb_log_flush_now='some';
ERROR HY000: Variable 'innodb_log_flush_now' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_log_flush_now=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_log_flush_now'
set global innodb_log_flush_now='foo';
ERROR 42000: Variable 'innodb_log_flush_now' can't be set to the value of 'foo'
set global innodb_log_flush_now=-2;
ERROR 42000: Variable 'innodb_log_flush_now' can't be set to the value of '-2'
set global innodb_log_flush_now=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_log_flush_now'
SET @@global.innodb_log_flush_now = @start_global_value;
SELECT @@global.innodb_log_flush_now;
@@global.innodb_log_flush_now
0
