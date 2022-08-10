SET @start_global_value = @@global.preload_buffer_size;
SELECT @start_global_value;
@start_global_value
32768
SET @start_session_value = @@session.preload_buffer_size;
SELECT @start_session_value;
@start_session_value
32768
'#--------------------FN_DYNVARS_129_01-------------------------#'
SET @@global.preload_buffer_size = 10000;
SET @@global.preload_buffer_size = DEFAULT;
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
32768
SET @@session.preload_buffer_size = 20000;
SET @@session.preload_buffer_size = DEFAULT;
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
32768
'#--------------------FN_DYNVARS_129_02-------------------------#'
SET @@global.preload_buffer_size = DEFAULT;
SELECT @@global.preload_buffer_size = 32768;
@@global.preload_buffer_size = 32768
1
SET @@session.preload_buffer_size = DEFAULT;
SELECT @@session.preload_buffer_size = 32768;
@@session.preload_buffer_size = 32768
1
'#--------------------FN_DYNVARS_129_03-------------------------#'
SET @@global.preload_buffer_size = 1024;
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
1024
SET @@global.preload_buffer_size = 1025;
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
1025
SET @@global.preload_buffer_size = 1073741824;
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
1073741824
SET @@global.preload_buffer_size = 1073741823;
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
1073741823
SET @@global.preload_buffer_size = 65536;
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
65536
'#--------------------FN_DYNVARS_129_04-------------------------#'
SET @@session.preload_buffer_size = 1024;
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
1024
SET @@session.preload_buffer_size = 1025;
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
1025
SET @@session.preload_buffer_size = 1073741824;
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
1073741824
SET @@session.preload_buffer_size = 1073741823;
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
1073741823
SET @@session.preload_buffer_size = 655536;
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
655536
'#------------------FN_DYNVARS_129_05-----------------------#'
SET @@global.preload_buffer_size = 64;
Warnings:
Warning	1292	Truncated incorrect preload_buffer_size value: '64'
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
1024
SET @@global.preload_buffer_size = -1;
Warnings:
Warning	1292	Truncated incorrect preload_buffer_size value: '-1'
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
1024
SET @@global.preload_buffer_size = 1023;
Warnings:
Warning	1292	Truncated incorrect preload_buffer_size value: '1023'
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
1024
SET @@global.preload_buffer_size = 1073741825;
Warnings:
Warning	1292	Truncated incorrect preload_buffer_size value: '1073741825'
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
1073741824
SET @@global.preload_buffer_size = 65530.34;
ERROR 42000: Incorrect argument type to variable 'preload_buffer_size'
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
1073741824
SET @@global.preload_buffer_size = test;
ERROR 42000: Incorrect argument type to variable 'preload_buffer_size'
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
1073741824
SET @@session.preload_buffer_size = 64;
Warnings:
Warning	1292	Truncated incorrect preload_buffer_size value: '64'
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
1024
SET @@session.preload_buffer_size = -2;
Warnings:
Warning	1292	Truncated incorrect preload_buffer_size value: '-2'
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
1024
SET @@session.preload_buffer_size = 65530.34;
ERROR 42000: Incorrect argument type to variable 'preload_buffer_size'
SET @@session.preload_buffer_size = 1023;
Warnings:
Warning	1292	Truncated incorrect preload_buffer_size value: '1023'
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
1024
SET @@session.preload_buffer_size = 1073741825;
Warnings:
Warning	1292	Truncated incorrect preload_buffer_size value: '1073741825'
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
1073741824
SET @@session.preload_buffer_size = test;
ERROR 42000: Incorrect argument type to variable 'preload_buffer_size'
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
1073741824
'#------------------FN_DYNVARS_129_06-----------------------#'
SELECT @@global.preload_buffer_size = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='preload_buffer_size';
@@global.preload_buffer_size = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_129_07-----------------------#'
SELECT @@session.preload_buffer_size = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='preload_buffer_size';
@@session.preload_buffer_size = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_129_08-----------------------#'
SET @@global.preload_buffer_size = TRUE;
Warnings:
Warning	1292	Truncated incorrect preload_buffer_size value: '1'
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
1024
SET @@global.preload_buffer_size = FALSE;
Warnings:
Warning	1292	Truncated incorrect preload_buffer_size value: '0'
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
1024
'#---------------------FN_DYNVARS_129_09----------------------#'
SET @@global.preload_buffer_size = 2048;
SELECT @@preload_buffer_size = @@global.preload_buffer_size;
@@preload_buffer_size = @@global.preload_buffer_size
0
'#---------------------FN_DYNVARS_129_10----------------------#'
SET @@preload_buffer_size = 5000;
SELECT @@preload_buffer_size = @@local.preload_buffer_size;
@@preload_buffer_size = @@local.preload_buffer_size
1
SELECT @@local.preload_buffer_size = @@session.preload_buffer_size;
@@local.preload_buffer_size = @@session.preload_buffer_size
1
'#---------------------FN_DYNVARS_129_11----------------------#'
SET preload_buffer_size = 1024;
SELECT @@preload_buffer_size;
@@preload_buffer_size
1024
SELECT local.preload_buffer_size;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.preload_buffer_size;
ERROR 42S02: Unknown table 'session' in field list
SELECT preload_buffer_size = @@session.preload_buffer_size;
ERROR 42S22: Unknown column 'preload_buffer_size' in 'field list'
'#---------------------FN_DYNVARS_129_12----------------------#'
CREATE USER foo, bar;
GRANT SESSION_VARIABLES_ADMIN ON *.*  TO bar;
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
2048
SET @@session.preload_buffer_size = 2048;
ERROR 42000: Access denied; you need (at least one of) the SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege(s) for this operation
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
2048
SET @@session.preload_buffer_size = 2048;
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
2048
SET @@global.preload_buffer_size = 2048;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
DROP USER foo, bar;
SET @@global.preload_buffer_size = @start_global_value;
SELECT @@global.preload_buffer_size;
@@global.preload_buffer_size
32768
SET @@session.preload_buffer_size = @start_session_value;
SELECT @@session.preload_buffer_size;
@@session.preload_buffer_size
32768
