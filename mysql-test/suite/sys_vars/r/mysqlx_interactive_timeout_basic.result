SET @start_global_value = @@global.mysqlx_interactive_timeout;
SELECT @start_global_value;
@start_global_value
28800
SET @@global.mysqlx_interactive_timeout = 100;
SET @@global.mysqlx_interactive_timeout = DEFAULT;
SELECT @@global.mysqlx_interactive_timeout;
@@global.mysqlx_interactive_timeout
28800
SET @@session.mysqlx_interactive_timeout = DEFAULT;
ERROR HY000: Variable 'mysqlx_interactive_timeout' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global.mysqlx_interactive_timeout = DEFAULT;
SELECT @@global.mysqlx_interactive_timeout = 28800;
@@global.mysqlx_interactive_timeout = 28800
1
SET @@global.mysqlx_interactive_timeout = 1;
SELECT @@global.mysqlx_interactive_timeout;
@@global.mysqlx_interactive_timeout
1
SET @@global.mysqlx_interactive_timeout = 60020;
SELECT @@global.mysqlx_interactive_timeout;
@@global.mysqlx_interactive_timeout
60020
SET @@global.mysqlx_interactive_timeout = 65535;
SELECT @@global.mysqlx_interactive_timeout;
@@global.mysqlx_interactive_timeout
65535
SET @@global.mysqlx_interactive_timeout = 31536000;
Warnings:
Warning	1292	Truncated incorrect mysqlx_interactive_timeout value: '31536000'
SELECT @@global.mysqlx_interactive_timeout;
@@global.mysqlx_interactive_timeout
2147483
SET @@global.mysqlx_interactive_timeout = 0;
Warnings:
Warning	1292	Truncated incorrect mysqlx_interactive_timeout value: '0'
SELECT @@global.mysqlx_interactive_timeout;
@@global.mysqlx_interactive_timeout
1
SET @@global.mysqlx_interactive_timeout = -1024;
Warnings:
Warning	1292	Truncated incorrect mysqlx_interactive_timeout value: '-1024'
SELECT @@global.mysqlx_interactive_timeout;
@@global.mysqlx_interactive_timeout
1
SET @@global.mysqlx_interactive_timeout = 31536001;
Warnings:
Warning	1292	Truncated incorrect mysqlx_interactive_timeout value: '31536001'
SELECT @@global.mysqlx_interactive_timeout;
@@global.mysqlx_interactive_timeout
2147483
SET @@global.mysqlx_interactive_timeout = 1000000000;
Warnings:
Warning	1292	Truncated incorrect mysqlx_interactive_timeout value: '1000000000'
SELECT @@global.mysqlx_interactive_timeout;
@@global.mysqlx_interactive_timeout
2147483
SET @@global.mysqlx_interactive_timeout = 65530.34;
ERROR 42000: Incorrect argument type to variable 'mysqlx_interactive_timeout'
SELECT @@global.mysqlx_interactive_timeout;
@@global.mysqlx_interactive_timeout
2147483
SET @@global.mysqlx_interactive_timeout = test;
ERROR 42000: Incorrect argument type to variable 'mysqlx_interactive_timeout'
SELECT @@global.mysqlx_interactive_timeout;
@@global.mysqlx_interactive_timeout
2147483
SELECT @@global.mysqlx_interactive_timeout = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='mysqlx_interactive_timeout';
@@global.mysqlx_interactive_timeout = VARIABLE_VALUE
1
SET @@global.mysqlx_interactive_timeout = TRUE;
SELECT @@global.mysqlx_interactive_timeout;
@@global.mysqlx_interactive_timeout
1
SET @@global.mysqlx_interactive_timeout = FALSE;
Warnings:
Warning	1292	Truncated incorrect mysqlx_interactive_timeout value: '0'
SELECT @@global.mysqlx_interactive_timeout;
@@global.mysqlx_interactive_timeout
1
SET @@global.mysqlx_interactive_timeout = 10;
SELECT @@mysqlx_interactive_timeout = @@global.mysqlx_interactive_timeout;
@@mysqlx_interactive_timeout = @@global.mysqlx_interactive_timeout
1
SET @@mysqlx_interactive_timeout = 100;
ERROR HY000: Variable 'mysqlx_interactive_timeout' is a GLOBAL variable and should be set with SET GLOBAL
SET mysqlx_interactive_timeout = 1;
ERROR HY000: Variable 'mysqlx_interactive_timeout' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global.mysqlx_interactive_timeout = @start_global_value;
