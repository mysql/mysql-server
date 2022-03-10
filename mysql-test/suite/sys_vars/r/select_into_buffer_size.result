SET @start_global_value = @@global.select_into_buffer_size;
SELECT @start_global_value;
@start_global_value
131072
SET @start_session_value = @@session.select_into_buffer_size;
SELECT @start_session_value;
@start_session_value
131072
'#--------------------FN_DYNVARS_002_01-------------------------#'
SET @@global.select_into_buffer_size = 1024;
Warnings:
Warning	1292	Truncated incorrect select_into_buffer_size value: '1024'
SET @@global.select_into_buffer_size = DEFAULT;
SELECT @@global.select_into_buffer_size;
@@global.select_into_buffer_size
131072
SET @@session.select_into_buffer_size = 2048;
Warnings:
Warning	1292	Truncated incorrect select_into_buffer_size value: '2048'
SET @@session.select_into_buffer_size = DEFAULT;
SELECT @@session.select_into_buffer_size;
@@session.select_into_buffer_size
131072
'#--------------------FN_DYNVARS_002_02-------------------------#'
SET @@global.select_into_buffer_size = @start_global_value;
SELECT @@global.select_into_buffer_size = 4096;
@@global.select_into_buffer_size = 4096
0
SET @@session.select_into_buffer_size = @start_session_value;
SELECT @@session.select_into_buffer_size = 2048;
@@session.select_into_buffer_size = 2048
0
'#--------------------FN_DYNVARS_002_03-------------------------#'
SET @@global.select_into_buffer_size = 16384;
SELECT @@global.select_into_buffer_size;
@@global.select_into_buffer_size
16384
SET @@global.select_into_buffer_size = 4096;
Warnings:
Warning	1292	Truncated incorrect select_into_buffer_size value: '4096'
SELECT @@global.select_into_buffer_size;
@@global.select_into_buffer_size
8192
SET @@global.select_into_buffer_size = 8192;
SELECT @@global.select_into_buffer_size;
@@global.select_into_buffer_size
8192
'#--------------------FN_DYNVARS_002_04-------------------------#'
SET @@session.select_into_buffer_size = 4096;
Warnings:
Warning	1292	Truncated incorrect select_into_buffer_size value: '4096'
SELECT @@session.select_into_buffer_size;
@@session.select_into_buffer_size
8192
SET @@session.select_into_buffer_size = 8192;
SELECT @@session.select_into_buffer_size;
@@session.select_into_buffer_size
8192
SET @@session.select_into_buffer_size = 16384;
SELECT @@session.select_into_buffer_size;
@@session.select_into_buffer_size
16384
'#------------------FN_DYNVARS_002_05-----------------------#'
SET @@global.select_into_buffer_size = -1;
Warnings:
Warning	1292	Truncated incorrect select_into_buffer_size value: '-1'
SELECT @@global.select_into_buffer_size;
@@global.select_into_buffer_size
8192
SET @@global.select_into_buffer_size = -1024;
Warnings:
Warning	1292	Truncated incorrect select_into_buffer_size value: '-1024'
SELECT @@global.select_into_buffer_size;
@@global.select_into_buffer_size
8192
SET @@global.select_into_buffer_size = 13;
Warnings:
Warning	1292	Truncated incorrect select_into_buffer_size value: '13'
SELECT @@global.select_into_buffer_size;
@@global.select_into_buffer_size
8192
SET @@global.select_into_buffer_size = ON;
ERROR 42000: Incorrect argument type to variable 'select_into_buffer_size'
SELECT @@global.select_into_buffer_size;
@@global.select_into_buffer_size
8192
SET @@global.select_into_buffer_size = OFF;
ERROR 42000: Incorrect argument type to variable 'select_into_buffer_size'
SELECT @@global.select_into_buffer_size;
@@global.select_into_buffer_size
8192
SET @@global.select_into_buffer_size = test;
ERROR 42000: Incorrect argument type to variable 'select_into_buffer_size'
SELECT @@global.select_into_buffer_size;
@@global.select_into_buffer_size
8192
SET @@session.select_into_buffer_size = 23;
Warnings:
Warning	1292	Truncated incorrect select_into_buffer_size value: '23'
SELECT @@session.select_into_buffer_size;
@@session.select_into_buffer_size
8192
SET @@session.select_into_buffer_size = -2;
Warnings:
Warning	1292	Truncated incorrect select_into_buffer_size value: '-2'
SELECT @@session.select_into_buffer_size;
@@session.select_into_buffer_size
8192
SET @@session.select_into_buffer_size = 65550;
Warnings:
Warning	1292	Truncated incorrect select_into_buffer_size value: '65550'
SELECT @@session.select_into_buffer_size;
@@session.select_into_buffer_size
65536
SET @@session.select_into_buffer_size = ON;
ERROR 42000: Incorrect argument type to variable 'select_into_buffer_size'
SELECT @@session.select_into_buffer_size;
@@session.select_into_buffer_size
65536
SET @@session.select_into_buffer_size = OFF;
ERROR 42000: Incorrect argument type to variable 'select_into_buffer_size'
SELECT @@session.select_into_buffer_size;
@@session.select_into_buffer_size
65536
SET @@session.select_into_buffer_size = test;
ERROR 42000: Incorrect argument type to variable 'select_into_buffer_size'
SELECT @@session.select_into_buffer_size;
@@session.select_into_buffer_size
65536
'#------------------FN_DYNVARS_002_06-----------------------#'
SELECT @@global.select_into_buffer_size = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='select_into_buffer_size';
@@global.select_into_buffer_size = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_002_07-----------------------#'
SELECT @@session.select_into_buffer_size = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='select_into_buffer_size';
@@session.select_into_buffer_size = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_002_08-----------------------#'
SET @@global.select_into_buffer_size = TRUE;
Warnings:
Warning	1292	Truncated incorrect select_into_buffer_size value: '1'
SELECT @@global.select_into_buffer_size;
@@global.select_into_buffer_size
8192
SET @@global.select_into_buffer_size = FALSE;
Warnings:
Warning	1292	Truncated incorrect select_into_buffer_size value: '0'
SELECT @@global.select_into_buffer_size;
@@global.select_into_buffer_size
8192
'#---------------------FN_DYNVARS_001_09----------------------#'
SET @@global.select_into_buffer_size = 16384;
SET @@session.select_into_buffer_size = 8192;
SELECT @@select_into_buffer_size = @@global.select_into_buffer_size;
@@select_into_buffer_size = @@global.select_into_buffer_size
0
'#---------------------FN_DYNVARS_001_10----------------------#'
SET @@select_into_buffer_size = 16384;
SELECT @@select_into_buffer_size = @@local.select_into_buffer_size;
@@select_into_buffer_size = @@local.select_into_buffer_size
1
SELECT @@local.select_into_buffer_size = @@session.select_into_buffer_size;
@@local.select_into_buffer_size = @@session.select_into_buffer_size
1
'#---------------------FN_DYNVARS_001_11----------------------#'
SET select_into_buffer_size = 1024;
Warnings:
Warning	1292	Truncated incorrect select_into_buffer_size value: '1024'
SELECT @@select_into_buffer_size;
@@select_into_buffer_size
8192
SELECT local.select_into_buffer_size;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.select_into_buffer_size;
ERROR 42S02: Unknown table 'session' in field list
SELECT select_into_buffer_size = @@session.select_into_buffer_size;
ERROR 42S22: Unknown column 'select_into_buffer_size' in 'field list'
'#---------------------FN_DYNVARS_001_12----------------------#'
CREATE USER foo, bar;
GRANT SESSION_VARIABLES_ADMIN ON *.*  TO bar;
SELECT @@session.select_into_buffer_size;
@@session.select_into_buffer_size
16384
SET @@session.select_into_buffer_size = 15000;
ERROR 42000: Access denied; you need (at least one of) the SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege(s) for this operation
SELECT @@session.select_into_buffer_size;
@@session.select_into_buffer_size
16384
SET @@session.select_into_buffer_size = 15000;
Warnings:
Warning	1292	Truncated incorrect select_into_buffer_size value: '15000'
SELECT @@session.select_into_buffer_size;
@@session.select_into_buffer_size
12288
SET @@global.select_into_buffer_size = 15000;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
DROP USER foo, bar;
SET @@global.select_into_buffer_size = @start_global_value;
SELECT @@global.select_into_buffer_size;
@@global.select_into_buffer_size
131072
SET @@session.select_into_buffer_size = @start_session_value;
SELECT @@session.select_into_buffer_size;
@@session.select_into_buffer_size
131072
