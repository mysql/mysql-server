SET @start_global_value = @@global.wait_timeout;
SET @start_session_value = @@session.wait_timeout;
'#--------------------FN_DYNVARS_005_01-------------------------#'
SET @@global.wait_timeout = 100;
SET @@global.wait_timeout = DEFAULT;
SELECT @@global.wait_timeout;
@@global.wait_timeout
28800
SET @@session.wait_timeout = 200;
SET @@session.wait_timeout = DEFAULT;
SELECT @@session.wait_timeout;
@@session.wait_timeout
28800
'#--------------------FN_DYNVARS_005_02-------------------------#'
SET @@global.wait_timeout = DEFAULT;
SELECT @@global.wait_timeout = @default_wait_timeout;
@@global.wait_timeout = @default_wait_timeout
1
SET @@session.wait_timeout = DEFAULT;
SELECT @@session.wait_timeout = @default_wait_timeout;
@@session.wait_timeout = @default_wait_timeout
1
'#--------------------FN_DYNVARS_005_03-------------------------#'
SET @@global.wait_timeout= 1;
SELECT @@global.wait_timeout;
@@global.wait_timeout
1
SET @@global.wait_timeout = 60020;
SELECT @@global.wait_timeout;
@@global.wait_timeout
60020
SET @@global.wait_timeout = 31536000;
SELECT @@global.wait_timeout = @max_wait_timeout;
@@global.wait_timeout = @max_wait_timeout
1
'#--------------------FN_DYNVARS_005_04-------------------------#'
SET @@session.wait_timeout =6000;
SELECT @@session.wait_timeout;
@@session.wait_timeout
6000
'#------------------FN_DYNVARS_005_05-----------------------#'
SET @@global.wait_timeout = 0;
Warnings:
Warning	1292	Truncated incorrect wait_timeout value: '0'
SELECT @@global.wait_timeout;
@@global.wait_timeout
1
SET @@global.wait_timeout = -1024;
Warnings:
Warning	1292	Truncated incorrect wait_timeout value: '-1024'
SELECT @@global.wait_timeout;
@@global.wait_timeout
1
SET @@global.wait_timeout = 123456789031;
Warnings:
Warning	1292	Truncated incorrect wait_timeout value: '123456789031'
SELECT @@global.wait_timeout;
@@global.wait_timeout
31536000
SET @@global.wait_timeout = 31536003;
Warnings:
Warning	1292	Truncated incorrect wait_timeout value: '31536003'
SELECT @@global.wait_timeout;
@@global.wait_timeout
31536000
SET @@global.wait_timeout = ' ';
ERROR 42000: Incorrect argument type to variable 'wait_timeout'
SELECT @@global.wait_timeout;
@@global.wait_timeout
31536000
SET @@global.wait_timeout = ON;
ERROR 42000: Incorrect argument type to variable 'wait_timeout'
SET @@global.wait_timeout = OFF;
ERROR 42000: Incorrect argument type to variable 'wait_timeout'
SET @@global.wait_timeout = True;
SELECT @@global.wait_timeout;
@@global.wait_timeout
1
SET @@global.wait_timeout = False;
Warnings:
Warning	1292	Truncated incorrect wait_timeout value: '0'
SELECT @@global.wait_timeout;
@@global.wait_timeout
1
SET @@global.wait_timeout = 65530.34;
ERROR 42000: Incorrect argument type to variable 'wait_timeout'
SELECT @@global.wait_timeout;
@@global.wait_timeout
1
SET @@global.wait_timeout ="Test";
ERROR 42000: Incorrect argument type to variable 'wait_timeout'
SET @@session.wait_timeout = ON;
ERROR 42000: Incorrect argument type to variable 'wait_timeout'
SET @@session.wait_timeout = OFF;
ERROR 42000: Incorrect argument type to variable 'wait_timeout'
SET @@session.wait_timeout = True;
SELECT @@session.wait_timeout;
@@session.wait_timeout
1
SET @@session.wait_timeout = False;
Warnings:
Warning	1292	Truncated incorrect wait_timeout value: '0'
SELECT @@session.wait_timeout;
@@session.wait_timeout
1
SET @@session.wait_timeout = "Test";
ERROR 42000: Incorrect argument type to variable 'wait_timeout'
SET @@session.wait_timeout = 65530.34;
ERROR 42000: Incorrect argument type to variable 'wait_timeout'
SELECT @@session.wait_timeout;
@@session.wait_timeout
1
SET @@session.wait_timeout = 'test';
ERROR 42000: Incorrect argument type to variable 'wait_timeout'
SET @@session.wait_timeout = 0;
Warnings:
Warning	1292	Truncated incorrect wait_timeout value: '0'
SELECT @@session.wait_timeout;
@@session.wait_timeout
1
SELECT @@session.wait_timeout = @max_wait_timeout;
@@session.wait_timeout = @max_wait_timeout
0
SELECT @@session.wait_timeout;
@@session.wait_timeout
1
SET @@session.wait_timeout = ' ';
ERROR 42000: Incorrect argument type to variable 'wait_timeout'
SELECT @@session.wait_timeout;
@@session.wait_timeout
1
'#------------------FN_DYNVARS_005_06-----------------------#'
SELECT @@global.wait_timeout = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='wait_timeout';
@@global.wait_timeout = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_005_07-----------------------#'
SELECT @@session.wait_timeout = VARIABLE_VALUE 
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='wait_timeout';
@@session.wait_timeout = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_001_09----------------------#'
SET @@global.wait_timeout = 30000;
SET @@global.wait_timeout = 40000;
SELECT @@wait_timeout = @@global.wait_timeout;
@@wait_timeout = @@global.wait_timeout
0
'#---------------------FN_DYNVARS_001_10----------------------#'
SET @@wait_timeout = 100;
SELECT @@wait_timeout = @@local.wait_timeout;
@@wait_timeout = @@local.wait_timeout
1
SELECT @@local.wait_timeout = @@session.wait_timeout;
@@local.wait_timeout = @@session.wait_timeout
1
'#---------------------FN_DYNVARS_001_11----------------------#'
SET wait_timeout = 1027;
SELECT @@wait_timeout;
@@wait_timeout
1027
SELECT local.wait_timeout;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.wait_timeout;
ERROR 42S02: Unknown table 'session' in field list
SELECT wait_timeout = @@session.wait_timeout;
ERROR 42S22: Unknown column 'wait_timeout' in 'field list'
SET @@global.wait_timeout = @start_global_value;
SET @@session.wait_timeout = @start_session_value;
