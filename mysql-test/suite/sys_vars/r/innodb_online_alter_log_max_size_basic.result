SET @start_global_value = @@global.innodb_online_alter_log_max_size;
SELECT @start_global_value;
@start_global_value
134217728
select @@global.innodb_online_alter_log_max_size >= 524288;
@@global.innodb_online_alter_log_max_size >= 524288
1
select @@global.innodb_online_alter_log_max_size;
@@global.innodb_online_alter_log_max_size
134217728
select @@session.innodb_online_alter_log_max_size;
ERROR HY000: Variable 'innodb_online_alter_log_max_size' is a GLOBAL variable
show global variables like 'innodb_online_alter_log_max_size';
Variable_name	Value
innodb_online_alter_log_max_size	134217728
show session variables like 'innodb_online_alter_log_max_size';
Variable_name	Value
innodb_online_alter_log_max_size	134217728
select * from performance_schema.global_variables where variable_name='innodb_online_alter_log_max_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_online_alter_log_max_size	134217728
select * from performance_schema.session_variables where variable_name='innodb_online_alter_log_max_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_online_alter_log_max_size	134217728
set global innodb_online_alter_log_max_size=1048576;
select @@global.innodb_online_alter_log_max_size;
@@global.innodb_online_alter_log_max_size
1048576
select * from performance_schema.global_variables where variable_name='innodb_online_alter_log_max_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_online_alter_log_max_size	1048576
select * from performance_schema.session_variables where variable_name='innodb_online_alter_log_max_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_online_alter_log_max_size	1048576
set @@global.innodb_online_alter_log_max_size=524288;
select @@global.innodb_online_alter_log_max_size;
@@global.innodb_online_alter_log_max_size
524288
select * from performance_schema.global_variables where variable_name='innodb_online_alter_log_max_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_online_alter_log_max_size	524288
select * from performance_schema.session_variables where variable_name='innodb_online_alter_log_max_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_online_alter_log_max_size	524288
set session innodb_online_alter_log_max_size='some';
ERROR HY000: Variable 'innodb_online_alter_log_max_size' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.innodb_online_alter_log_max_size='some';
ERROR HY000: Variable 'innodb_online_alter_log_max_size' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_online_alter_log_max_size=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_online_alter_log_max_size'
set global innodb_online_alter_log_max_size='foo';
ERROR 42000: Incorrect argument type to variable 'innodb_online_alter_log_max_size'
set global innodb_online_alter_log_max_size=-2;
Warnings:
Warning	1292	Truncated incorrect innodb_online_alter_log_max_size value: '-2'
set global innodb_online_alter_log_max_size=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_online_alter_log_max_size'
set global innodb_online_alter_log_max_size=2;
Warnings:
Warning	1292	Truncated incorrect innodb_online_alter_log_max_size value: '2'
SET @@global.innodb_online_alter_log_max_size = @start_global_value;
SELECT @@global.innodb_online_alter_log_max_size;
@@global.innodb_online_alter_log_max_size
134217728
