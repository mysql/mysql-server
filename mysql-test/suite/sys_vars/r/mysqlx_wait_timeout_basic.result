SET @start_global_value = @@global.mysqlx_wait_timeout;
SET @start_session_value = @@session.mysqlx_wait_timeout;
SET @@global.mysqlx_wait_timeout = 100;
SET @@global.mysqlx_wait_timeout = DEFAULT;
SELECT @@global.mysqlx_wait_timeout;
@@global.mysqlx_wait_timeout
28800
SET @@session.mysqlx_wait_timeout = 200;
SET @@session.mysqlx_wait_timeout = DEFAULT;
SELECT @@session.mysqlx_wait_timeout;
@@session.mysqlx_wait_timeout
28800
SET @@global.mysqlx_wait_timeout = DEFAULT;
SELECT @@global.mysqlx_wait_timeout = @default_mysqlx_wait_timeout;
@@global.mysqlx_wait_timeout = @default_mysqlx_wait_timeout
NULL
SET @@session.mysqlx_wait_timeout = DEFAULT;
SELECT @@session.mysqlx_wait_timeout = @default_mysqlx_wait_timeout;
@@session.mysqlx_wait_timeout = @default_mysqlx_wait_timeout
NULL
SET @@global.mysqlx_wait_timeout= 1;
SELECT @@global.mysqlx_wait_timeout;
@@global.mysqlx_wait_timeout
1
SET @@global.mysqlx_wait_timeout = 60020;
SELECT @@global.mysqlx_wait_timeout;
@@global.mysqlx_wait_timeout
60020
SET @@global.mysqlx_wait_timeout = 31536000;
Warnings:
Warning	1292	Truncated incorrect mysqlx_wait_timeout value: '31536000'
SELECT @@global.mysqlx_wait_timeout = @max_mysqlx_wait_timeout;
@@global.mysqlx_wait_timeout = @max_mysqlx_wait_timeout
NULL
SET @@session.mysqlx_wait_timeout =6000;
SELECT @@session.mysqlx_wait_timeout;
@@session.mysqlx_wait_timeout
6000
SET @@global.mysqlx_wait_timeout = 0;
Warnings:
Warning	1292	Truncated incorrect mysqlx_wait_timeout value: '0'
SELECT @@global.mysqlx_wait_timeout;
@@global.mysqlx_wait_timeout
1
SET @@global.mysqlx_wait_timeout = -1024;
Warnings:
Warning	1292	Truncated incorrect mysqlx_wait_timeout value: '-1024'
SELECT @@global.mysqlx_wait_timeout;
@@global.mysqlx_wait_timeout
1
SET @@global.mysqlx_wait_timeout = 123456789031;
Warnings:
Warning	1292	Truncated incorrect mysqlx_wait_timeout value: '123456789031'
SELECT @@global.mysqlx_wait_timeout;
@@global.mysqlx_wait_timeout
2147483
SET @@global.mysqlx_wait_timeout = 31536003;
Warnings:
Warning	1292	Truncated incorrect mysqlx_wait_timeout value: '31536003'
SELECT @@global.mysqlx_wait_timeout;
@@global.mysqlx_wait_timeout
2147483
SET @@global.mysqlx_wait_timeout = ' ';
ERROR 42000: Incorrect argument type to variable 'mysqlx_wait_timeout'
SELECT @@global.mysqlx_wait_timeout;
@@global.mysqlx_wait_timeout
2147483
SET @@global.mysqlx_wait_timeout = ON;
ERROR 42000: Incorrect argument type to variable 'mysqlx_wait_timeout'
SET @@global.mysqlx_wait_timeout = OFF;
ERROR 42000: Incorrect argument type to variable 'mysqlx_wait_timeout'
SET @@global.mysqlx_wait_timeout = True;
SELECT @@global.mysqlx_wait_timeout;
@@global.mysqlx_wait_timeout
1
SET @@global.mysqlx_wait_timeout = False;
Warnings:
Warning	1292	Truncated incorrect mysqlx_wait_timeout value: '0'
SELECT @@global.mysqlx_wait_timeout;
@@global.mysqlx_wait_timeout
1
SET @@global.mysqlx_wait_timeout = 65530.34;
ERROR 42000: Incorrect argument type to variable 'mysqlx_wait_timeout'
SELECT @@global.mysqlx_wait_timeout;
@@global.mysqlx_wait_timeout
1
SET @@global.mysqlx_wait_timeout ="Test";
ERROR 42000: Incorrect argument type to variable 'mysqlx_wait_timeout'
SET @@session.mysqlx_wait_timeout = ON;
ERROR 42000: Incorrect argument type to variable 'mysqlx_wait_timeout'
SET @@session.mysqlx_wait_timeout = OFF;
ERROR 42000: Incorrect argument type to variable 'mysqlx_wait_timeout'
SET @@session.mysqlx_wait_timeout = True;
SELECT @@session.mysqlx_wait_timeout;
@@session.mysqlx_wait_timeout
1
SET @@session.mysqlx_wait_timeout = False;
Warnings:
Warning	1292	Truncated incorrect mysqlx_wait_timeout value: '0'
SELECT @@session.mysqlx_wait_timeout;
@@session.mysqlx_wait_timeout
1
SET @@session.mysqlx_wait_timeout = "Test";
ERROR 42000: Incorrect argument type to variable 'mysqlx_wait_timeout'
SET @@session.mysqlx_wait_timeout = 65530.34;
ERROR 42000: Incorrect argument type to variable 'mysqlx_wait_timeout'
SELECT @@session.mysqlx_wait_timeout;
@@session.mysqlx_wait_timeout
1
SET @@session.mysqlx_wait_timeout = 'test';
ERROR 42000: Incorrect argument type to variable 'mysqlx_wait_timeout'
SET @@session.mysqlx_wait_timeout = 0;
Warnings:
Warning	1292	Truncated incorrect mysqlx_wait_timeout value: '0'
SELECT @@session.mysqlx_wait_timeout;
@@session.mysqlx_wait_timeout
1
SELECT @@session.mysqlx_wait_timeout = @max_mysqlx_wait_timeout;
@@session.mysqlx_wait_timeout = @max_mysqlx_wait_timeout
NULL
SELECT @@session.mysqlx_wait_timeout;
@@session.mysqlx_wait_timeout
1
SET @@session.mysqlx_wait_timeout = ' ';
ERROR 42000: Incorrect argument type to variable 'mysqlx_wait_timeout'
SELECT @@session.mysqlx_wait_timeout;
@@session.mysqlx_wait_timeout
1
SELECT @@global.mysqlx_wait_timeout = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='mysqlx_wait_timeout';
@@global.mysqlx_wait_timeout = VARIABLE_VALUE
1
SELECT @@session.mysqlx_wait_timeout = VARIABLE_VALUE 
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='mysqlx_wait_timeout';
@@session.mysqlx_wait_timeout = VARIABLE_VALUE
1
SET @@global.mysqlx_wait_timeout = 30000;
SET @@global.mysqlx_wait_timeout = 40000;
SELECT @@mysqlx_wait_timeout = @@global.mysqlx_wait_timeout;
@@mysqlx_wait_timeout = @@global.mysqlx_wait_timeout
0
SET @@mysqlx_wait_timeout = 100;
SELECT @@mysqlx_wait_timeout = @@local.mysqlx_wait_timeout;
@@mysqlx_wait_timeout = @@local.mysqlx_wait_timeout
1
SELECT @@local.mysqlx_wait_timeout = @@session.mysqlx_wait_timeout;
@@local.mysqlx_wait_timeout = @@session.mysqlx_wait_timeout
1
SET mysqlx_wait_timeout = 1027;
SELECT @@mysqlx_wait_timeout;
@@mysqlx_wait_timeout
1027
SELECT local.mysqlx_wait_timeout;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.mysqlx_wait_timeout;
ERROR 42S02: Unknown table 'session' in field list
SELECT mysqlx_wait_timeout = @@session.mysqlx_wait_timeout;
ERROR 42S22: Unknown column 'mysqlx_wait_timeout' in 'field list'
SET @@global.mysqlx_wait_timeout = @start_global_value;
SET @@session.mysqlx_wait_timeout = @start_session_value;
