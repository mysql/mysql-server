SET @start_global_value = @@global.net_buffer_length;
SET @@global.net_buffer_length = DEFAULT;
'#--------------------FN_DYNVARS_109_01-------------------------#'
SET @@global.net_buffer_length = 10000;
Warnings:
Warning	1292	Truncated incorrect net_buffer_length value: '10000'
SET @@global.net_buffer_length = DEFAULT;
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
16384
SET @@session.net_buffer_length = 20000;
ERROR HY000: SESSION variable 'net_buffer_length' is read-only. Use SET GLOBAL to assign the value
SET @@session.net_buffer_length = DEFAULT;
ERROR HY000: SESSION variable 'net_buffer_length' is read-only. Use SET GLOBAL to assign the value
SELECT @@session.net_buffer_length;
@@session.net_buffer_length
16384
'#--------------------FN_DYNVARS_109_02-------------------------#'
SET @@global.net_buffer_length = DEFAULT;
SELECT @@global.net_buffer_length = 16384;
@@global.net_buffer_length = 16384
1
'#--------------------FN_DYNVARS_109_03-------------------------#'
SET @@global.net_buffer_length = 1024;
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
1024
SET @@global.net_buffer_length = 1025;
Warnings:
Warning	1292	Truncated incorrect net_buffer_length value: '1025'
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
1024
SET @@global.net_buffer_length = 1048576;
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
1048576
SET @@global.net_buffer_length = 1048575;
Warnings:
Warning	1292	Truncated incorrect net_buffer_length value: '1048575'
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
1047552
SET @@global.net_buffer_length = 65535;
Warnings:
Warning	1292	Truncated incorrect net_buffer_length value: '65535'
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
64512
'#--------------------FN_DYNVARS_109_04-------------------------#'
'#------------------FN_DYNVARS_109_05-----------------------#'
SET @@global.net_buffer_length = 0;
Warnings:
Warning	1292	Truncated incorrect net_buffer_length value: '0'
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
1024
SET @@global.net_buffer_length = -1024;
Warnings:
Warning	1292	Truncated incorrect net_buffer_length value: '-1024'
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
1024
SET @@global.net_buffer_length = 1023;
Warnings:
Warning	1292	Truncated incorrect net_buffer_length value: '1023'
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
1024
SET @@global.net_buffer_length = 1048577;
Warnings:
Warning	1292	Truncated incorrect net_buffer_length value: '1048577'
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
1048576
SET @@global.net_buffer_length = 104857633;
Warnings:
Warning	1292	Truncated incorrect net_buffer_length value: '104857633'
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
1048576
SET @@global.net_buffer_length = 65530.34;
ERROR 42000: Incorrect argument type to variable 'net_buffer_length'
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
1048576
SET @@global.net_buffer_length = test;
ERROR 42000: Incorrect argument type to variable 'net_buffer_length'
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
1048576
SET @@global.net_buffer_length = ' ';
ERROR 42000: Incorrect argument type to variable 'net_buffer_length'
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
1048576
SET @@session.net_buffer_length = test;
ERROR 42000: Incorrect argument type to variable 'net_buffer_length'
SELECT @@session.net_buffer_length;
@@session.net_buffer_length
16384
'#------------------FN_DYNVARS_109_06-----------------------#'
SELECT @@global.net_buffer_length = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='net_buffer_length';
@@global.net_buffer_length = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_109_07-----------------------#'
SELECT @@session.net_buffer_length = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='net_buffer_length';
@@session.net_buffer_length = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_109_08-----------------------#'
SET @@global.net_buffer_length = TRUE;
Warnings:
Warning	1292	Truncated incorrect net_buffer_length value: '1'
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
1024
SET @@global.net_buffer_length = FALSE;
Warnings:
Warning	1292	Truncated incorrect net_buffer_length value: '0'
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
1024
'#---------------------FN_DYNVARS_109_09----------------------#'
SET @@global.net_buffer_length = 2048;
SELECT @@net_buffer_length = @@global.net_buffer_length;
@@net_buffer_length = @@global.net_buffer_length
0
'#---------------------FN_DYNVARS_109_10----------------------#'
'#---------------------FN_DYNVARS_109_11----------------------#'
SELECT @@net_buffer_length;
@@net_buffer_length
16384
SELECT local.net_buffer_length;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.net_buffer_length;
ERROR 42S02: Unknown table 'session' in field list
SELECT net_buffer_length = @@session.net_buffer_length;
ERROR 42S22: Unknown column 'net_buffer_length' in 'field list'
SET @@global.net_buffer_length = @start_global_value;
