SET @start_global_value = @@global.join_buffer_size;
SELECT @start_global_value;
@start_global_value
262144
SET @start_session_value = @@session.join_buffer_size;
SELECT @start_session_value;
@start_session_value
262144
'#--------------------FN_DYNVARS_053_01-------------------------#'
SET @@global.join_buffer_size = DEFAULT;
SELECT @@global.join_buffer_size;
@@global.join_buffer_size
262144
SET @@session.join_buffer_size = DEFAULT;
SELECT @@session.join_buffer_size;
@@session.join_buffer_size
262144
'#--------------------FN_DYNVARS_053_03-------------------------#'
SET @@global.join_buffer_size = 128;
SELECT @@global.join_buffer_size;
@@global.join_buffer_size
128
SET @@global.join_buffer_size = 8200;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '8200'
SELECT @@global.join_buffer_size;
@@global.join_buffer_size
8192
SET @@global.join_buffer_size = 65536;
SELECT @@global.join_buffer_size;
@@global.join_buffer_size
65536
SET @@global.join_buffer_size = 4294967295;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '4294967295'
SELECT @@global.join_buffer_size;
@@global.join_buffer_size
4294967168
SET @@global.join_buffer_size = 18446744073709547520;
SELECT @@global.join_buffer_size;
@@global.join_buffer_size
18446744073709547520
'#--------------------FN_DYNVARS_053_04-------------------------#'
SET @@session.join_buffer_size = 128;
SELECT @@session.join_buffer_size;
@@session.join_buffer_size
128
SET @@session.join_buffer_size = 8200;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '8200'
SELECT @@session.join_buffer_size;
@@session.join_buffer_size
8192
SET @@session.join_buffer_size = 65536;
SELECT @@session.join_buffer_size;
@@session.join_buffer_size
65536
SET @@session.join_buffer_size = 4294967295;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '4294967295'
SELECT @@session.join_buffer_size;
@@session.join_buffer_size
4294967168
SET @@session.join_buffer_size = 18446744073709547520;
SELECT @@session.join_buffer_size;
@@session.join_buffer_size
18446744073709547520
'#------------------FN_DYNVARS_053_05-----------------------#'
SET @@global.join_buffer_size = 0;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '0'
SELECT @@global.join_buffer_size;
@@global.join_buffer_size
128
SET @@global.join_buffer_size = -1024;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '-1024'
SELECT @@global.join_buffer_size;
@@global.join_buffer_size
128
SET @@global.join_buffer_size = 127;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '127'
SELECT @@global.join_buffer_size;
@@global.join_buffer_size
128
SET @@global.join_buffer_size = 65530.34;
ERROR 42000: Incorrect argument type to variable 'join_buffer_size'
SELECT @@global.join_buffer_size;
@@global.join_buffer_size
128
SET @@global.join_buffer_size = test;
ERROR 42000: Incorrect argument type to variable 'join_buffer_size'
SELECT @@global.join_buffer_size;
@@global.join_buffer_size
128
SET @@global.join_buffer_size = 18446744073709547521;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '18446744073709547521'
SELECT @@global.join_buffer_size;
@@global.join_buffer_size
18446744073709547520
SET @@session.join_buffer_size = 0;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '0'
SELECT @@session.join_buffer_size;
@@session.join_buffer_size
128
SET @@session.join_buffer_size = -1024;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '-1024'
SELECT @@session.join_buffer_size;
@@session.join_buffer_size
128
SET @@session.join_buffer_size = 127;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '127'
SELECT @@session.join_buffer_size;
@@session.join_buffer_size
128
SET @@session.join_buffer_size = 65530.34;
ERROR 42000: Incorrect argument type to variable 'join_buffer_size'
SELECT @@session.join_buffer_size;
@@session.join_buffer_size
128
SET @@session.join_buffer_size = test;
ERROR 42000: Incorrect argument type to variable 'join_buffer_size'
SELECT @@session.join_buffer_size;
@@session.join_buffer_size
128
SET @@session.join_buffer_size = 18446744073709547521;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '18446744073709547521'
SELECT @@session.join_buffer_size;
@@session.join_buffer_size
18446744073709547520
'#------------------FN_DYNVARS_053_06-----------------------#'
SET @@global.join_buffer_size = 4294967295;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '4294967295'
SELECT @@global.join_buffer_size = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='join_buffer_size';
@@global.join_buffer_size = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_053_07-----------------------#'
SET @@session.join_buffer_size = 4294967295;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '4294967295'
SELECT @@session.join_buffer_size = VARIABLE_VALUE 
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='join_buffer_size';
@@session.join_buffer_size = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_053_08-----------------------#'
SET @@global.join_buffer_size = TRUE;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '1'
SET @@global.join_buffer_size = FALSE;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '0'
'#---------------------FN_DYNVARS_001_09----------------------#'
SET @@global.join_buffer_size = 10;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '10'
SELECT @@join_buffer_size = @@global.join_buffer_size;
@@join_buffer_size = @@global.join_buffer_size
0
'#---------------------FN_DYNVARS_001_10----------------------#'
SET @@join_buffer_size = 100;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '100'
SELECT @@join_buffer_size = @@local.join_buffer_size;
@@join_buffer_size = @@local.join_buffer_size
1
SELECT @@local.join_buffer_size = @@session.join_buffer_size;
@@local.join_buffer_size = @@session.join_buffer_size
1
'#---------------------FN_DYNVARS_001_11----------------------#'
SET join_buffer_size = 1;
Warnings:
Warning	1292	Truncated incorrect join_buffer_size value: '1'
SELECT @@join_buffer_size;
@@join_buffer_size
128
SELECT local.join_buffer_size;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.join_buffer_size;
ERROR 42S02: Unknown table 'session' in field list
SELECT join_buffer_size = @@session.join_buffer_size;
ERROR 42S22: Unknown column 'join_buffer_size' in 'field list'
SET @@global.join_buffer_size = @start_global_value;
SELECT @@global.join_buffer_size;
@@global.join_buffer_size
262144
SET @@session.join_buffer_size = @start_session_value;
SELECT @@session.join_buffer_size;
@@session.join_buffer_size
262144
