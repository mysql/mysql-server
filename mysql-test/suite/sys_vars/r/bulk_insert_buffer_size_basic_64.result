SET @start_global_value = @@global.bulk_insert_buffer_size;
SELECT @start_global_value;
@start_global_value
8388608
SET @start_session_value = @@session.bulk_insert_buffer_size;
SELECT @start_session_value;
@start_session_value
8388608
'#--------------------FN_DYNVARS_007_01-------------------------#'
SET @@global.bulk_insert_buffer_size = 100;
SET @@global.bulk_insert_buffer_size = DEFAULT;
SELECT @@global.bulk_insert_buffer_size;
@@global.bulk_insert_buffer_size
8388608
SET @@session.bulk_insert_buffer_size = 200;
SET @@session.bulk_insert_buffer_size = DEFAULT;
SELECT @@session.bulk_insert_buffer_size;
@@session.bulk_insert_buffer_size
8388608
'#--------------------FN_DYNVARS_007_02-------------------------#'
SET @@global.bulk_insert_buffer_size = @start_global_value;
SELECT @@global.bulk_insert_buffer_size = 8388608;
@@global.bulk_insert_buffer_size = 8388608
1
SET @@session.bulk_insert_buffer_size = @start_session_value;
SELECT @@session.bulk_insert_buffer_size = 8388608;
@@session.bulk_insert_buffer_size = 8388608
1
'#--------------------FN_DYNVARS_007_03-------------------------#'
SET @@global.bulk_insert_buffer_size = 0;
SELECT @@global.bulk_insert_buffer_size;
@@global.bulk_insert_buffer_size
0
SET @@global.bulk_insert_buffer_size = 1;
SELECT @@global.bulk_insert_buffer_size;
@@global.bulk_insert_buffer_size
1
SET @@global.bulk_insert_buffer_size = 4294967295;
SELECT @@global.bulk_insert_buffer_size;
@@global.bulk_insert_buffer_size
4294967295
SET @@global.bulk_insert_buffer_size = 429496;
SELECT @@global.bulk_insert_buffer_size;
@@global.bulk_insert_buffer_size
429496
SET @@global.bulk_insert_buffer_size = 18446744073709551615;
SELECT @@global.bulk_insert_buffer_size;
@@global.bulk_insert_buffer_size
18446744073709551615
'#--------------------FN_DYNVARS_007_04-------------------------#'
SET @@session.bulk_insert_buffer_size = 0;
SELECT @@session.bulk_insert_buffer_size;
@@session.bulk_insert_buffer_size
0
SET @@session.bulk_insert_buffer_size = 1;
SELECT @@session.bulk_insert_buffer_size;
@@session.bulk_insert_buffer_size
1
SET @@session.bulk_insert_buffer_size = 4294967295;
SELECT @@session.bulk_insert_buffer_size;
@@session.bulk_insert_buffer_size
4294967295
SET @@session.bulk_insert_buffer_size = 429496;
SELECT @@session.bulk_insert_buffer_size;
@@session.bulk_insert_buffer_size
429496
SET @@session.bulk_insert_buffer_size = 18446744073709551615;
SELECT @@session.bulk_insert_buffer_size;
@@session.bulk_insert_buffer_size
18446744073709551615
'#------------------FN_DYNVARS_007_05-----------------------#'
SET @@global.bulk_insert_buffer_size = -1024;
Warnings:
Warning	1292	Truncated incorrect bulk_insert_buffer_size value: '-1024'
SELECT @@global.bulk_insert_buffer_size;
@@global.bulk_insert_buffer_size
0
SET @@global.bulk_insert_buffer_size = test;
ERROR 42000: Incorrect argument type to variable 'bulk_insert_buffer_size'
SET @@global.bulk_insert_buffer_size = ON;
ERROR 42000: Incorrect argument type to variable 'bulk_insert_buffer_size'
SET @@global.bulk_insert_buffer_size = 429496.10;
ERROR 42000: Incorrect argument type to variable 'bulk_insert_buffer_size'
SET @@global.bulk_insert_buffer_size = 18446744073709551616;
ERROR 42000: Incorrect argument type to variable 'bulk_insert_buffer_size'
SELECT @@global.bulk_insert_buffer_size;
@@global.bulk_insert_buffer_size
0
SET @@session.bulk_insert_buffer_size = -2;
Warnings:
Warning	1292	Truncated incorrect bulk_insert_buffer_size value: '-2'
SELECT @@session.bulk_insert_buffer_size;
@@session.bulk_insert_buffer_size
0
SET @@session.bulk_insert_buffer_size = test;
ERROR 42000: Incorrect argument type to variable 'bulk_insert_buffer_size'
SET @@session.bulk_insert_buffer_size = 429496.10;
ERROR 42000: Incorrect argument type to variable 'bulk_insert_buffer_size'
SET @@session.bulk_insert_buffer_size = 18446744073709551616;
ERROR 42000: Incorrect argument type to variable 'bulk_insert_buffer_size'
SELECT @@session.bulk_insert_buffer_size;
@@session.bulk_insert_buffer_size
0
'#------------------FN_DYNVARS_007_06-----------------------#'
SELECT @@global.bulk_insert_buffer_size = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='bulk_insert_buffer_size';
@@global.bulk_insert_buffer_size = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_007_07-----------------------#'
SELECT @@session.bulk_insert_buffer_size = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='bulk_insert_buffer_size';
@@session.bulk_insert_buffer_size = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_007_08-----------------------#'
SET @@global.bulk_insert_buffer_size = TRUE;
SELECT @@global.bulk_insert_buffer_size;
@@global.bulk_insert_buffer_size
1
SET @@global.bulk_insert_buffer_size = FALSE;
SELECT @@global.bulk_insert_buffer_size;
@@global.bulk_insert_buffer_size
0
SET @@session.bulk_insert_buffer_size = TRUE;
SELECT @@session.bulk_insert_buffer_size;
@@session.bulk_insert_buffer_size
1
SET @@session.bulk_insert_buffer_size = FALSE;
SELECT @@session.bulk_insert_buffer_size;
@@session.bulk_insert_buffer_size
0
'#---------------------FN_DYNVARS_007_09----------------------#'
SET @@bulk_insert_buffer_size = 100;
SELECT @@bulk_insert_buffer_size = @@local.bulk_insert_buffer_size;
@@bulk_insert_buffer_size = @@local.bulk_insert_buffer_size
1
SELECT @@local.bulk_insert_buffer_size = @@session.bulk_insert_buffer_size;
@@local.bulk_insert_buffer_size = @@session.bulk_insert_buffer_size
1
'#---------------------FN_DYNVARS_007_10----------------------#'
SET bulk_insert_buffer_size = 1;
SELECT @@bulk_insert_buffer_size;
@@bulk_insert_buffer_size
1
SET local.bulk_insert_buffer_size = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.bulk_insert_buffer_size = 1' at line 1
SELECT local.bulk_insert_buffer_size;
ERROR 42S02: Unknown table 'local' in field list
SET session.bulk_insert_buffer_size = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'session.bulk_insert_buffer_size = 1' at line 1
SELECT session.bulk_insert_buffer_size;
ERROR 42S02: Unknown table 'session' in field list
SELECT bulk_insert_buffer_size = @@session.bulk_insert_buffer_size;
ERROR 42S22: Unknown column 'bulk_insert_buffer_size' in 'field list'
SET @@global.bulk_insert_buffer_size = @start_global_value;
SELECT @@global.bulk_insert_buffer_size;
@@global.bulk_insert_buffer_size
8388608
SET @@session.bulk_insert_buffer_size = @start_session_value;
SELECT @@session.bulk_insert_buffer_size;
@@session.bulk_insert_buffer_size
8388608
