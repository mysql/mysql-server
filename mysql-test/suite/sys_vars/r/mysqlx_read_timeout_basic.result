SET @start_global_value = @@global.mysqlx_read_timeout;
SELECT @start_global_value;
@start_global_value
30
SET @@global.mysqlx_read_timeout = 100;
SET @@global.mysqlx_read_timeout = DEFAULT;
SELECT @@global.mysqlx_read_timeout;
@@global.mysqlx_read_timeout
30
SET @@session.mysqlx_read_timeout = DEFAULT;
SET @@global.mysqlx_read_timeout = DEFAULT;
SELECT @@global.mysqlx_read_timeout = 28800;
@@global.mysqlx_read_timeout = 28800
0
SET @@global.mysqlx_read_timeout = 1;
SELECT @@global.mysqlx_read_timeout;
@@global.mysqlx_read_timeout
1
SET @@global.mysqlx_read_timeout = 60020;
SELECT @@global.mysqlx_read_timeout;
@@global.mysqlx_read_timeout
60020
SET @@global.mysqlx_read_timeout = 65535;
SELECT @@global.mysqlx_read_timeout;
@@global.mysqlx_read_timeout
65535
SET @@global.mysqlx_read_timeout = 31536000;
Warnings:
Warning	1292	Truncated incorrect mysqlx_read_timeout value: '31536000'
SELECT @@global.mysqlx_read_timeout;
@@global.mysqlx_read_timeout
2147483
SET @@global.mysqlx_read_timeout = 0;
Warnings:
Warning	1292	Truncated incorrect mysqlx_read_timeout value: '0'
SELECT @@global.mysqlx_read_timeout;
@@global.mysqlx_read_timeout
1
SET @@global.mysqlx_read_timeout = -1024;
Warnings:
Warning	1292	Truncated incorrect mysqlx_read_timeout value: '-1024'
SELECT @@global.mysqlx_read_timeout;
@@global.mysqlx_read_timeout
1
SET @@global.mysqlx_read_timeout = 31536001;
Warnings:
Warning	1292	Truncated incorrect mysqlx_read_timeout value: '31536001'
SELECT @@global.mysqlx_read_timeout;
@@global.mysqlx_read_timeout
2147483
SET @@global.mysqlx_read_timeout = 1000000000;
Warnings:
Warning	1292	Truncated incorrect mysqlx_read_timeout value: '1000000000'
SELECT @@global.mysqlx_read_timeout;
@@global.mysqlx_read_timeout
2147483
SET @@global.mysqlx_read_timeout = 65530.34;
ERROR 42000: Incorrect argument type to variable 'mysqlx_read_timeout'
SELECT @@global.mysqlx_read_timeout;
@@global.mysqlx_read_timeout
2147483
SET @@global.mysqlx_read_timeout = test;
ERROR 42000: Incorrect argument type to variable 'mysqlx_read_timeout'
SELECT @@global.mysqlx_read_timeout;
@@global.mysqlx_read_timeout
2147483
SELECT @@global.mysqlx_read_timeout = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='mysqlx_read_timeout';
@@global.mysqlx_read_timeout = VARIABLE_VALUE
1
SET @@global.mysqlx_read_timeout = TRUE;
SELECT @@global.mysqlx_read_timeout;
@@global.mysqlx_read_timeout
1
SET @@global.mysqlx_read_timeout = FALSE;
Warnings:
Warning	1292	Truncated incorrect mysqlx_read_timeout value: '0'
SELECT @@global.mysqlx_read_timeout;
@@global.mysqlx_read_timeout
1
SET @@global.mysqlx_read_timeout = 10;
SELECT @@mysqlx_read_timeout = @@global.mysqlx_read_timeout;
@@mysqlx_read_timeout = @@global.mysqlx_read_timeout
0
SET @@mysqlx_read_timeout = 100;
SET mysqlx_read_timeout = 1;
SET @@global.mysqlx_read_timeout = @start_global_value;
