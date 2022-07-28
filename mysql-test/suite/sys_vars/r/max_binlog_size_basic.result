SET @start_value = @@global.max_binlog_size;
SELECT @start_value;
@start_value
1073741824
'#--------------------FN_DYNVARS_072_01------------------------#'
SET @@global.max_binlog_size = 5000;
Warnings:
Warning	1292	Truncated incorrect max_binlog_size value: '5000'
SET @@global.max_binlog_size = DEFAULT;
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
1073741824
'#---------------------FN_DYNVARS_072_02-------------------------#'
SET @@global.max_binlog_size = @start_value;
SELECT @@global.max_binlog_size = 1073741824;
@@global.max_binlog_size = 1073741824
1
'#--------------------FN_DYNVARS_072_03------------------------#'
SET @@global.max_binlog_size = 4096;
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
4096
SET @@global.max_binlog_size = 1073741824;
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
1073741824
SET @@global.max_binlog_size = 1073741823;
Warnings:
Warning	1292	Truncated incorrect max_binlog_size value: '1073741823'
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
1073737728
SET @@global.max_binlog_size = 4097;
Warnings:
Warning	1292	Truncated incorrect max_binlog_size value: '4097'
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
4096
SET @@global.max_binlog_size = 65535;
Warnings:
Warning	1292	Truncated incorrect max_binlog_size value: '65535'
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
61440
'#--------------------FN_DYNVARS_072_04-------------------------#'
SET @@global.max_binlog_size = -1;
Warnings:
Warning	1292	Truncated incorrect max_binlog_size value: '-1'
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
4096
SET @@global.max_binlog_size = 100000000000;
Warnings:
Warning	1292	Truncated incorrect max_binlog_size value: '100000000000'
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
1073741824
SET @@global.max_binlog_size = 10000.01;
ERROR 42000: Incorrect argument type to variable 'max_binlog_size'
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
1073741824
SET @@global.max_binlog_size = -1024;
Warnings:
Warning	1292	Truncated incorrect max_binlog_size value: '-1024'
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
4096
SET @@global.max_binlog_size = 1024;
Warnings:
Warning	1292	Truncated incorrect max_binlog_size value: '1024'
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
4096
SET @@global.max_binlog_size = 4294967296;
Warnings:
Warning	1292	Truncated incorrect max_binlog_size value: '4294967296'
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
1073741824
SET @@global.max_binlog_size = 4095;
Warnings:
Warning	1292	Truncated incorrect max_binlog_size value: '4095'
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
4096
SET @@global.max_binlog_size = ON;
ERROR 42000: Incorrect argument type to variable 'max_binlog_size'
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
4096
SET @@global.max_binlog_size = 'test';
ERROR 42000: Incorrect argument type to variable 'max_binlog_size'
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
4096
'#-------------------FN_DYNVARS_072_05----------------------------#'
SET @@session.max_binlog_size = 4096;
ERROR HY000: Variable 'max_binlog_size' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@session.max_binlog_size;
ERROR HY000: Variable 'max_binlog_size' is a GLOBAL variable
'#----------------------FN_DYNVARS_072_06------------------------#'
SELECT @@global.max_binlog_size = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='max_binlog_size';
@@global.max_binlog_size = VARIABLE_VALUE
1
SELECT @@max_binlog_size = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='max_binlog_size';
@@max_binlog_size = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_072_07----------------------#'
SET @@global.max_binlog_size = TRUE;
Warnings:
Warning	1292	Truncated incorrect max_binlog_size value: '1'
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
4096
SET @@global.max_binlog_size = FALSE;
Warnings:
Warning	1292	Truncated incorrect max_binlog_size value: '0'
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
4096
'#---------------------FN_DYNVARS_072_08----------------------#'
SET @@global.max_binlog_size = 5000;
Warnings:
Warning	1292	Truncated incorrect max_binlog_size value: '5000'
SELECT @@max_binlog_size = @@global.max_binlog_size;
@@max_binlog_size = @@global.max_binlog_size
1
'#---------------------FN_DYNVARS_072_09----------------------#'
SET max_binlog_size = 6000;
ERROR HY000: Variable 'max_binlog_size' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@max_binlog_size;
@@max_binlog_size
4096
SET local.max_binlog_size = 7000;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.max_binlog_size = 7000' at line 1
SELECT local.max_binlog_size;
ERROR 42S02: Unknown table 'local' in field list
SET global.max_binlog_size = 8000;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.max_binlog_size = 8000' at line 1
SELECT global.max_binlog_size;
ERROR 42S02: Unknown table 'global' in field list
SELECT max_binlog_size;
ERROR 42S22: Unknown column 'max_binlog_size' in 'field list'
SELECT @@session.max_binlog_size;
ERROR HY000: Variable 'max_binlog_size' is a GLOBAL variable
SET @@global.max_binlog_size = @start_value;
SELECT @@global.max_binlog_size;
@@global.max_binlog_size
1073741824
