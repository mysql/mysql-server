SET @start_global_value = @@global.mysqlx_write_timeout;
SELECT @start_global_value;
@start_global_value
60
SET @@global.mysqlx_write_timeout = 100;
SET @@global.mysqlx_write_timeout = DEFAULT;
SELECT @@global.mysqlx_write_timeout;
@@global.mysqlx_write_timeout
60
SET @@session.mysqlx_write_timeout = DEFAULT;
SET @@global.mysqlx_write_timeout = DEFAULT;
SELECT @@global.mysqlx_write_timeout = 28800;
@@global.mysqlx_write_timeout = 28800
0
SET @@global.mysqlx_write_timeout = 1;
SELECT @@global.mysqlx_write_timeout;
@@global.mysqlx_write_timeout
1
SET @@global.mysqlx_write_timeout = 60020;
SELECT @@global.mysqlx_write_timeout;
@@global.mysqlx_write_timeout
60020
SET @@global.mysqlx_write_timeout = 65535;
SELECT @@global.mysqlx_write_timeout;
@@global.mysqlx_write_timeout
65535
SET @@global.mysqlx_write_timeout = 31536000;
Warnings:
Warning	1292	Truncated incorrect mysqlx_write_timeout value: '31536000'
SELECT @@global.mysqlx_write_timeout;
@@global.mysqlx_write_timeout
2147483
SET @@global.mysqlx_write_timeout = 0;
Warnings:
Warning	1292	Truncated incorrect mysqlx_write_timeout value: '0'
SELECT @@global.mysqlx_write_timeout;
@@global.mysqlx_write_timeout
1
SET @@global.mysqlx_write_timeout = -1024;
Warnings:
Warning	1292	Truncated incorrect mysqlx_write_timeout value: '-1024'
SELECT @@global.mysqlx_write_timeout;
@@global.mysqlx_write_timeout
1
SET @@global.mysqlx_write_timeout = 31536001;
Warnings:
Warning	1292	Truncated incorrect mysqlx_write_timeout value: '31536001'
SELECT @@global.mysqlx_write_timeout;
@@global.mysqlx_write_timeout
2147483
SET @@global.mysqlx_write_timeout = 1000000000;
Warnings:
Warning	1292	Truncated incorrect mysqlx_write_timeout value: '1000000000'
SELECT @@global.mysqlx_write_timeout;
@@global.mysqlx_write_timeout
2147483
SET @@global.mysqlx_write_timeout = 65530.34;
ERROR 42000: Incorrect argument type to variable 'mysqlx_write_timeout'
SELECT @@global.mysqlx_write_timeout;
@@global.mysqlx_write_timeout
2147483
SET @@global.mysqlx_write_timeout = test;
ERROR 42000: Incorrect argument type to variable 'mysqlx_write_timeout'
SELECT @@global.mysqlx_write_timeout;
@@global.mysqlx_write_timeout
2147483
SELECT @@global.mysqlx_write_timeout = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='mysqlx_write_timeout';
@@global.mysqlx_write_timeout = VARIABLE_VALUE
1
SET @@global.mysqlx_write_timeout = TRUE;
SELECT @@global.mysqlx_write_timeout;
@@global.mysqlx_write_timeout
1
SET @@global.mysqlx_write_timeout = FALSE;
Warnings:
Warning	1292	Truncated incorrect mysqlx_write_timeout value: '0'
SELECT @@global.mysqlx_write_timeout;
@@global.mysqlx_write_timeout
1
SET @@global.mysqlx_write_timeout = 10;
SELECT @@mysqlx_write_timeout = @@global.mysqlx_write_timeout;
@@mysqlx_write_timeout = @@global.mysqlx_write_timeout
0
SET @@mysqlx_write_timeout = 100;
SET mysqlx_write_timeout = 1;
SET @@global.mysqlx_write_timeout = @start_global_value;
