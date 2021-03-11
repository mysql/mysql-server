SET @start_value = @@global.slow_launch_time;
SELECT @start_value;
@start_value
2
'#--------------------FN_DYNVARS_150_01------------------------#'
SET @@global.slow_launch_time = 99;
SET @@global.slow_launch_time = DEFAULT;
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
2
'#---------------------FN_DYNVARS_150_02-------------------------#'
SET @@global.slow_launch_time = @start_value;
SELECT @@global.slow_launch_time = 2;
@@global.slow_launch_time = 2
1
'#--------------------FN_DYNVARS_150_03------------------------#'
SET @@global.slow_launch_time = 0;
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
0
SET @@global.slow_launch_time = 1;
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
1
SET @@global.slow_launch_time = 31536000;
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
31536000
SET @@global.slow_launch_time = 31535999;
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
31535999
SET @@global.slow_launch_time = 65536;
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
65536
'#--------------------FN_DYNVARS_150_04-------------------------#'
SET @@global.slow_launch_time = -1;
Warnings:
Warning	1292	Truncated incorrect slow_launch_time value: '-1'
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
0
SET @@global.slow_launch_time = 31536001;
Warnings:
Warning	1292	Truncated incorrect slow_launch_time value: '31536001'
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
31536000
SET @@global.slow_launch_time = 10240022115;
Warnings:
Warning	1292	Truncated incorrect slow_launch_time value: '10240022115'
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
31536000
SET @@global.slow_launch_time = 10000.01;
ERROR 42000: Incorrect argument type to variable 'slow_launch_time'
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
31536000
SET @@global.slow_launch_time = -1024;
Warnings:
Warning	1292	Truncated incorrect slow_launch_time value: '-1024'
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
0
SET @@global.slow_launch_time = 42949672950;
Warnings:
Warning	1292	Truncated incorrect slow_launch_time value: '42949672950'
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
31536000
SET @@global.slow_launch_time = ON;
ERROR 42000: Incorrect argument type to variable 'slow_launch_time'
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
31536000
SET @@global.slow_launch_time = 'test';
ERROR 42000: Incorrect argument type to variable 'slow_launch_time'
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
31536000
'#-------------------FN_DYNVARS_150_05----------------------------#'
SET @@session.slow_launch_time = 0;
ERROR HY000: Variable 'slow_launch_time' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@slow_launch_time;
@@slow_launch_time
31536000
'#----------------------FN_DYNVARS_150_06------------------------#'
SELECT @@global.slow_launch_time = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='slow_launch_time';
@@global.slow_launch_time = VARIABLE_VALUE
1
SELECT @@slow_launch_time = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='slow_launch_time';
@@slow_launch_time = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_150_07----------------------#'
SET @@global.slow_launch_time = TRUE;
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
1
SET @@global.slow_launch_time = FALSE;
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
0
'#---------------------FN_DYNVARS_150_08----------------------#'
SET @@global.slow_launch_time = 1;
SELECT @@slow_launch_time = @@global.slow_launch_time;
@@slow_launch_time = @@global.slow_launch_time
1
'#---------------------FN_DYNVARS_150_09----------------------#'
SET slow_launch_time = 1;
ERROR HY000: Variable 'slow_launch_time' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@slow_launch_time;
@@slow_launch_time
1
SET local.slow_launch_time = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.slow_launch_time = 1' at line 1
SELECT local.slow_launch_time;
ERROR 42S02: Unknown table 'local' in field list
SET global.slow_launch_time = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.slow_launch_time = 1' at line 1
SELECT global.slow_launch_time;
ERROR 42S02: Unknown table 'global' in field list
SELECT slow_launch_time;
ERROR 42S22: Unknown column 'slow_launch_time' in 'field list'
SELECT @@session.slow_launch_time;
ERROR HY000: Variable 'slow_launch_time' is a GLOBAL variable
SET @@global.slow_launch_time = @start_value;
SELECT @@global.slow_launch_time;
@@global.slow_launch_time
2
