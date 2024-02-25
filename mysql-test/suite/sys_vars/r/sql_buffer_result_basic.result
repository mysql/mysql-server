SET @session_start_value = @@session.sql_buffer_result;
SELECT @session_start_value;
@session_start_value
0
'#--------------------FN_DYNVARS_155_01------------------------#'
SET @@session.sql_buffer_result = 0;
SET @@session.sql_buffer_result = DEFAULT;
SELECT @@session.sql_buffer_result;
@@session.sql_buffer_result
0
SET @@session.sql_buffer_result = 1;
SET @@session.sql_buffer_result = DEFAULT;
SELECT @@session.sql_buffer_result;
@@session.sql_buffer_result
0
'#---------------------FN_DYNVARS_155_02-------------------------#'
SET sql_buffer_result = 1;
SELECT @@sql_buffer_result;
@@sql_buffer_result
1
SELECT session.sql_buffer_result;
ERROR 42S02: Unknown table 'session' in field list
SELECT local.sql_buffer_result;
ERROR 42S02: Unknown table 'local' in field list
SET session sql_buffer_result = 0;
SELECT @@session.sql_buffer_result;
@@session.sql_buffer_result
0
'#--------------------FN_DYNVARS_155_03------------------------#'
SET @@session.sql_buffer_result = 0;
SELECT @@session.sql_buffer_result;
@@session.sql_buffer_result
0
SET @@session.sql_buffer_result = 1;
SELECT @@session.sql_buffer_result;
@@session.sql_buffer_result
1
'#--------------------FN_DYNVARS_155_04-------------------------#'
SET @@session.sql_buffer_result = -1;
ERROR 42000: Variable 'sql_buffer_result' can't be set to the value of '-1'
SET @@session.sql_buffer_result = 2;
ERROR 42000: Variable 'sql_buffer_result' can't be set to the value of '2'
SET @@session.sql_buffer_result = "TRU";
ERROR 42000: Variable 'sql_buffer_result' can't be set to the value of 'TRU'
SET @@session.sql_buffer_result = 0.4;
ERROR 42000: Incorrect argument type to variable 'sql_buffer_result'
SET @@session.sql_buffer_result = TRÜE;
ERROR 42000: Variable 'sql_buffer_result' can't be set to the value of 'TRÜE'
SET @@session.sql_buffer_result = ÕN;
ERROR 42000: Variable 'sql_buffer_result' can't be set to the value of 'ÕN'
SET @@session.sql_buffer_result = OF;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'OF' at line 1
SET @@session.sql_buffer_result = ÓFF;
ERROR 42000: Variable 'sql_buffer_result' can't be set to the value of 'ÓFF'
SET @@session.sql_buffer_result = '¹';
ERROR 42000: Variable 'sql_buffer_result' can't be set to the value of '¹'
SET @@session.sql_buffer_result = NO;
ERROR 42000: Variable 'sql_buffer_result' can't be set to the value of 'NO'
'#-------------------FN_DYNVARS_155_05----------------------------#'
SET @@global.sql_buffer_result = 1-@@global.sql_buffer_result;
SELECT @@global.sql_buffer_result;
@@global.sql_buffer_result
1
SET @@global.sql_buffer_result = 1-@@global.sql_buffer_result;
SELECT @@global.sql_buffer_result;
@@global.sql_buffer_result
0
'#----------------------FN_DYNVARS_155_06------------------------#'
SELECT count(VARIABLE_VALUE) 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='sql_buffer_result';
count(VARIABLE_VALUE)
1
'#----------------------FN_DYNVARS_155_07------------------------#'
SELECT IF(@@session.sql_buffer_result, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='sql_buffer_result';
IF(@@session.sql_buffer_result, "ON", "OFF") = VARIABLE_VALUE
1
SELECT @@session.sql_buffer_result;
@@session.sql_buffer_result
1
SELECT VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='sql_buffer_result';
VARIABLE_VALUE
ON
'#---------------------FN_DYNVARS_155_08-------------------------#'
SET @@session.sql_buffer_result = OFF;
SELECT @@session.sql_buffer_result;
@@session.sql_buffer_result
0
SET @@session.sql_buffer_result = ON;
SELECT @@session.sql_buffer_result;
@@session.sql_buffer_result
1
'#---------------------FN_DYNVARS_155_09----------------------#'
SET @@session.sql_buffer_result = TRUE;
SELECT @@session.sql_buffer_result;
@@session.sql_buffer_result
1
SET @@session.sql_buffer_result = FALSE;
SELECT @@session.sql_buffer_result;
@@session.sql_buffer_result
0
SET @@session.sql_buffer_result = @session_start_value;
SELECT @@session.sql_buffer_result;
@@session.sql_buffer_result
0
