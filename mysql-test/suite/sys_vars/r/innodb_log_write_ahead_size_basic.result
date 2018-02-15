SET @start_global_value = @@global.innodb_log_write_ahead_size;
SET global innodb_log_write_ahead_size=4096;
Valid values are positive number
SELECT @@global.innodb_log_write_ahead_size >= 512;
@@global.innodb_log_write_ahead_size >= 512
1
SELECT @@global.innodb_log_write_ahead_size <= 16*1024;
@@global.innodb_log_write_ahead_size <= 16*1024
1
SELECT @@session.innodb_log_write_ahead_size;
ERROR HY000: Variable 'innodb_log_write_ahead_size' is a GLOBAL variable
SHOW global variables LIKE 'innodb_log_write_ahead_size';
Variable_name	Value
innodb_log_write_ahead_size	4096
SHOW session variables LIKE 'innodb_log_write_ahead_size';
Variable_name	Value
innodb_log_write_ahead_size	4096
SELECT * FROM performance_schema.global_variables
WHERE variable_name='innodb_log_write_ahead_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_write_ahead_size	4096
SELECT * FROM performance_schema.session_variables
WHERE variable_name='innodb_log_write_ahead_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_write_ahead_size	4096
SET global innodb_log_write_ahead_size=1024;
SELECT @@global.innodb_log_write_ahead_size;
@@global.innodb_log_write_ahead_size
1024
SELECT * FROM performance_schema.global_variables
WHERE variable_name='innodb_log_write_ahead_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_write_ahead_size	1024
SELECT * FROM performance_schema.session_variables
WHERE variable_name='innodb_log_write_ahead_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_write_ahead_size	1024
SET session innodb_log_write_ahead_size=2048;
ERROR HY000: Variable 'innodb_log_write_ahead_size' is a GLOBAL variable and should be set with SET GLOBAL
SET global innodb_log_write_ahead_size=512;
SELECT @@global.innodb_log_write_ahead_size;
@@global.innodb_log_write_ahead_size
512
SET global innodb_log_write_ahead_size=2048;
SELECT @@global.innodb_log_write_ahead_size;
@@global.innodb_log_write_ahead_size
2048
SET global innodb_log_write_ahead_size=4096;
SELECT @@global.innodb_log_write_ahead_size;
@@global.innodb_log_write_ahead_size
4096
SET global innodb_log_write_ahead_size=0;
Warnings:
Warning	1292	Truncated incorrect innodb_log_write_ahead_size value: '0'
SELECT @@global.innodb_log_write_ahead_size;
@@global.innodb_log_write_ahead_size
512
SET global innodb_log_write_ahead_size=-1024;
Warnings:
Warning	1292	Truncated incorrect innodb_log_write_ahead_size value: '-1024'
SELECT @@global.innodb_log_write_ahead_size;
@@global.innodb_log_write_ahead_size
512
SET global innodb_log_write_ahead_size=3000;
Warnings:
Warning	1292	Truncated incorrect innodb_log_write_ahead_size value: '3000'
Warning	1210	innodb_log_write_ahead_size should be set to power of 2, in range [512,16384]
Warning	1210	Setting innodb_log_write_ahead_size to 4096
SELECT @@global.innodb_log_write_ahead_size;
@@global.innodb_log_write_ahead_size
4096
SET global innodb_log_write_ahead_size=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_log_write_ahead_size'
SET global innodb_log_write_ahead_size=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_log_write_ahead_size'
SET global innodb_log_write_ahead_size="foo";
ERROR 42000: Incorrect argument type to variable 'innodb_log_write_ahead_size'
SET global innodb_log_write_ahead_size=-7;
Warnings:
Warning	1292	Truncated incorrect innodb_log_write_ahead_size value: '-7'
SELECT @@global.innodb_log_write_ahead_size;
@@global.innodb_log_write_ahead_size
512
SELECT * FROM performance_schema.global_variables
WHERE variable_name='innodb_log_write_ahead_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_log_write_ahead_size	512
SET @@global.innodb_log_write_ahead_size = @start_global_value;
