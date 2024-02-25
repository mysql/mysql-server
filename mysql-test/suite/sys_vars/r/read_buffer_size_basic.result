SET @start_global_value = @@global.read_buffer_size;
SELECT @start_global_value;
@start_global_value
131072
SET @start_session_value = @@session.read_buffer_size;
SELECT @start_session_value;
@start_session_value
131072
'#--------------------FN_DYNVARS_138_01-------------------------#'
SET @@global.read_buffer_size = 1000;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '1000'
SET @@global.read_buffer_size = DEFAULT;
SELECT @@global.read_buffer_size;
@@global.read_buffer_size
131072
SET @@session.read_buffer_size = 2000;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '2000'
SET @@session.read_buffer_size = DEFAULT;
SELECT @@session.read_buffer_size;
@@session.read_buffer_size
131072
'#--------------------FN_DYNVARS_138_02-------------------------#'
SET @@global.read_buffer_size = DEFAULT;
SELECT @@global.read_buffer_size = 131072;
@@global.read_buffer_size = 131072
1
SET @@session.read_buffer_size = DEFAULT;
SELECT @@session.read_buffer_size = 131072;
@@session.read_buffer_size = 131072
1
'#--------------------FN_DYNVARS_138_03-------------------------#'
SET @@global.read_buffer_size = 8201;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '8201'
SELECT @@global.read_buffer_size;
@@global.read_buffer_size
8192
SET @@global.read_buffer_size = 8200;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '8200'
SELECT @@global.read_buffer_size;
@@global.read_buffer_size
8192
SET @@global.read_buffer_size = 2147479552;
SELECT @@global.read_buffer_size;
@@global.read_buffer_size
2147479552
SET @@global.read_buffer_size = 2147479551;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '2147479551'
SELECT @@global.read_buffer_size;
@@global.read_buffer_size
2147475456
'#--------------------FN_DYNVARS_138_04-------------------------#'
SET @@session.read_buffer_size = 8200;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '8200'
SELECT @@session.read_buffer_size;
@@session.read_buffer_size
8192
SET @@session.read_buffer_size = 8201;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '8201'
SELECT @@session.read_buffer_size;
@@session.read_buffer_size
8192
SET @@session.read_buffer_size = 2147479552;
SELECT @@session.read_buffer_size;
@@session.read_buffer_size
2147479552
SET @@session.read_buffer_size = 2147479551;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '2147479551'
SELECT @@session.read_buffer_size;
@@session.read_buffer_size
2147475456
'#------------------FN_DYNVARS_138_05-----------------------#'
SET @@global.read_buffer_size = 8199;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '8199'
SELECT @@global.read_buffer_size;
@@global.read_buffer_size
8192
SET @@global.read_buffer_size = -1024;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '-1024'
SELECT @@global.read_buffer_size;
@@global.read_buffer_size
8192
SET @@global.read_buffer_size = 2147479553;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '2147479553'
SELECT @@global.read_buffer_size;
@@global.read_buffer_size
2147479552
SET @@global.read_buffer_size = 65530.34;
ERROR 42000: Incorrect argument type to variable 'read_buffer_size'
SELECT @@global.read_buffer_size;
@@global.read_buffer_size
2147479552
SET @@global.read_buffer_size = test;
ERROR 42000: Incorrect argument type to variable 'read_buffer_size'
SELECT @@global.read_buffer_size;
@@global.read_buffer_size
2147479552
SET @@session.read_buffer_size = 8199;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '8199'
SELECT @@session.read_buffer_size;
@@session.read_buffer_size
8192
SET @@session.read_buffer_size = -2;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '-2'
SELECT @@session.read_buffer_size;
@@session.read_buffer_size
8192
SET @@session.read_buffer_size = 65530.34;
ERROR 42000: Incorrect argument type to variable 'read_buffer_size'
SET @@session.read_buffer_size = 2147479553;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '2147479553'
SELECT @@session.read_buffer_size;
@@session.read_buffer_size
2147479552
SET @@session.read_buffer_size = test;
ERROR 42000: Incorrect argument type to variable 'read_buffer_size'
SELECT @@session.read_buffer_size;
@@session.read_buffer_size
2147479552
'#------------------FN_DYNVARS_138_06-----------------------#'
SELECT @@global.read_buffer_size = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='read_buffer_size';
@@global.read_buffer_size = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_138_07-----------------------#'
SELECT @@session.read_buffer_size = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='read_buffer_size';
@@session.read_buffer_size = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_138_08-----------------------#'
SET @@global.read_buffer_size = TRUE;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '1'
SELECT @@global.read_buffer_size;
@@global.read_buffer_size
8192
SET @@global.read_buffer_size = FALSE;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '0'
SELECT @@global.read_buffer_size;
@@global.read_buffer_size
8192
'#---------------------FN_DYNVARS_138_09----------------------#'
SET @@global.read_buffer_size = 9000;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '9000'
SELECT @@read_buffer_size = @@global.read_buffer_size;
@@read_buffer_size = @@global.read_buffer_size
0
'#---------------------FN_DYNVARS_138_10----------------------#'
SET @@read_buffer_size = 9000;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '9000'
SELECT @@read_buffer_size = @@local.read_buffer_size;
@@read_buffer_size = @@local.read_buffer_size
1
SELECT @@local.read_buffer_size = @@session.read_buffer_size;
@@local.read_buffer_size = @@session.read_buffer_size
1
'#---------------------FN_DYNVARS_138_11----------------------#'
SET read_buffer_size = 9100;
Warnings:
Warning	1292	Truncated incorrect read_buffer_size value: '9100'
SELECT @@read_buffer_size;
@@read_buffer_size
8192
SELECT local.read_buffer_size;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.read_buffer_size;
ERROR 42S02: Unknown table 'session' in field list
SELECT read_buffer_size = @@session.read_buffer_size;
ERROR 42S22: Unknown column 'read_buffer_size' in 'field list'
SET @@global.read_buffer_size = @start_global_value;
SELECT @@global.read_buffer_size;
@@global.read_buffer_size
131072
SET @@session.read_buffer_size = @start_session_value;
SELECT @@session.read_buffer_size;
@@session.read_buffer_size
131072
