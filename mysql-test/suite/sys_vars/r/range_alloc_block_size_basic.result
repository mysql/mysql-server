SET @start_global_value = @@global.range_alloc_block_size;
SELECT @start_global_value;
@start_global_value
4096
SET @start_session_value = @@session.range_alloc_block_size;
SELECT @start_session_value;
@start_session_value
4096
'#--------------------FN_DYNVARS_137_01-------------------------#'
SET @@global.range_alloc_block_size = 100;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '100'
SET @@global.range_alloc_block_size = DEFAULT;
SELECT @@global.range_alloc_block_size;
@@global.range_alloc_block_size
4096
SET @@session.range_alloc_block_size = 200;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '200'
SET @@session.range_alloc_block_size = DEFAULT;
SELECT @@session.range_alloc_block_size;
@@session.range_alloc_block_size
4096
'#--------------------FN_DYNVARS_137_02-------------------------#'
SET @@global.range_alloc_block_size = DEFAULT;
SELECT @@global.range_alloc_block_size = 2048;
@@global.range_alloc_block_size = 2048
0
SET @@session.range_alloc_block_size = DEFAULT;
SELECT @@session.range_alloc_block_size = 2048;
@@session.range_alloc_block_size = 2048
0
'#--------------------FN_DYNVARS_137_03-------------------------#'
SET @@global.range_alloc_block_size = 2048;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '2048'
SELECT @@global.range_alloc_block_size;
@@global.range_alloc_block_size
4096
SET @@global.range_alloc_block_size = 4294967295;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '4294967295'
SELECT @@global.range_alloc_block_size;
@@global.range_alloc_block_size
4294966272
SET @@global.range_alloc_block_size = 4294967294;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '4294967294'
SELECT @@global.range_alloc_block_size;
@@global.range_alloc_block_size
4294966272
'#--------------------FN_DYNVARS_137_04-------------------------#'
SET @@session.range_alloc_block_size = 2048;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '2048'
SELECT @@session.range_alloc_block_size;
@@session.range_alloc_block_size
4096
SET @@session.range_alloc_block_size = 4294967295;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '4294967295'
SELECT @@session.range_alloc_block_size;
@@session.range_alloc_block_size
4294966272
SET @@session.range_alloc_block_size = 4294967294;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '4294967294'
SELECT @@session.range_alloc_block_size;
@@session.range_alloc_block_size
4294966272
'#------------------FN_DYNVARS_137_05-----------------------#'
SET @@global.range_alloc_block_size = 0;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '0'
SELECT @@global.range_alloc_block_size;
@@global.range_alloc_block_size
4096
SET @@global.range_alloc_block_size = -1024;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '-1024'
SELECT @@global.range_alloc_block_size;
@@global.range_alloc_block_size
4096
SET @@global.range_alloc_block_size = 42949672951;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '42949672951'
SELECT @@global.range_alloc_block_size;
@@global.range_alloc_block_size
4294966272
SET @@global.range_alloc_block_size = 65530.34;
ERROR 42000: Incorrect argument type to variable 'range_alloc_block_size'
SELECT @@global.range_alloc_block_size;
@@global.range_alloc_block_size
4294966272
SET @@global.range_alloc_block_size = test;
ERROR 42000: Incorrect argument type to variable 'range_alloc_block_size'
SELECT @@global.range_alloc_block_size;
@@global.range_alloc_block_size
4294966272
SET @@session.range_alloc_block_size = 0;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '0'
SELECT @@session.range_alloc_block_size;
@@session.range_alloc_block_size
4096
SET @@session.range_alloc_block_size = -2;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '-2'
SELECT @@session.range_alloc_block_size;
@@session.range_alloc_block_size
4096
SET @@session.range_alloc_block_size = 65530.34;
ERROR 42000: Incorrect argument type to variable 'range_alloc_block_size'
SET @@session.range_alloc_block_size = 4294967296;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '4294967296'
SELECT @@session.range_alloc_block_size;
@@session.range_alloc_block_size
4294966272
SET @@session.range_alloc_block_size = test;
ERROR 42000: Incorrect argument type to variable 'range_alloc_block_size'
SELECT @@session.range_alloc_block_size;
@@session.range_alloc_block_size
4294966272
'#------------------FN_DYNVARS_137_06-----------------------#'
SELECT @@global.range_alloc_block_size = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='range_alloc_block_size';
@@global.range_alloc_block_size = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_137_07-----------------------#'
SELECT @@session.range_alloc_block_size = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='range_alloc_block_size';
@@session.range_alloc_block_size = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_137_08-----------------------#'
SET @@global.range_alloc_block_size = TRUE;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '1'
SELECT @@global.range_alloc_block_size;
@@global.range_alloc_block_size
4096
SET @@global.range_alloc_block_size = FALSE;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '0'
SELECT @@global.range_alloc_block_size;
@@global.range_alloc_block_size
4096
'#---------------------FN_DYNVARS_137_09----------------------#'
SET @@global.range_alloc_block_size = 10;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '10'
SELECT @@range_alloc_block_size = @@global.range_alloc_block_size;
@@range_alloc_block_size = @@global.range_alloc_block_size
0
'#---------------------FN_DYNVARS_137_10----------------------#'
SET @@range_alloc_block_size = 100;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '100'
SELECT @@range_alloc_block_size = @@local.range_alloc_block_size;
@@range_alloc_block_size = @@local.range_alloc_block_size
1
SELECT @@local.range_alloc_block_size = @@session.range_alloc_block_size;
@@local.range_alloc_block_size = @@session.range_alloc_block_size
1
'#---------------------FN_DYNVARS_137_11----------------------#'
SET range_alloc_block_size = 1;
Warnings:
Warning	1292	Truncated incorrect range_alloc_block_size value: '1'
SELECT @@range_alloc_block_size;
@@range_alloc_block_size
4096
SELECT local.range_alloc_block_size;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.range_alloc_block_size;
ERROR 42S02: Unknown table 'session' in field list
SELECT range_alloc_block_size = @@session.range_alloc_block_size;
ERROR 42S22: Unknown column 'range_alloc_block_size' in 'field list'
SET @@global.range_alloc_block_size = @start_global_value;
SELECT @@global.range_alloc_block_size;
@@global.range_alloc_block_size
4096
SET @@session.range_alloc_block_size = @start_session_value;
SELECT @@session.range_alloc_block_size;
@@session.range_alloc_block_size
4096
