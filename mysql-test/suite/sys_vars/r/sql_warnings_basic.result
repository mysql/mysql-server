SET @session_start_value = @@session.sql_warnings;
SELECT @session_start_value;
@session_start_value
0
'#--------------------FN_DYNVARS_166_01------------------------#'
SET @@session.sql_warnings = 0;
SET @@session.sql_warnings = DEFAULT;
SELECT @@session.sql_warnings;
@@session.sql_warnings
0
SET @@session.sql_warnings = 1;
SET @@session.sql_warnings = DEFAULT;
SELECT @@session.sql_warnings;
@@session.sql_warnings
0
'#---------------------FN_DYNVARS_166_02-------------------------#'
SET sql_warnings = 1;
SELECT @@sql_warnings;
@@sql_warnings
1
SELECT session.sql_warnings;
ERROR 42S02: Unknown table 'session' in field list
SELECT local.sql_warnings;
ERROR 42S02: Unknown table 'local' in field list
SET session sql_warnings = 0;
SELECT @@session.sql_warnings;
@@session.sql_warnings
0
'#--------------------FN_DYNVARS_166_03------------------------#'
SET @@session.sql_warnings = 0;
SELECT @@session.sql_warnings;
@@session.sql_warnings
0
SET @@session.sql_warnings = 1;
SELECT @@session.sql_warnings;
@@session.sql_warnings
1
'#--------------------FN_DYNVARS_166_04-------------------------#'
SET @@session.sql_warnings = 0.6;
ERROR 42000: Incorrect argument type to variable 'sql_warnings'
SET @@session.sql_warnings = "T";
ERROR 42000: Variable 'sql_warnings' can't be set to the value of 'T'
SET @@session.sql_warnings = "Y";
ERROR 42000: Variable 'sql_warnings' can't be set to the value of 'Y'
SET @@session.sql_warnings = TRÜE;
ERROR 42000: Variable 'sql_warnings' can't be set to the value of 'TRÜE'
SET @@session.sql_warnings = ÕN;
ERROR 42000: Variable 'sql_warnings' can't be set to the value of 'ÕN'
SET @@session.sql_warnings = OF;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'OF' at line 1
SET @@session.sql_warnings = ÓFF;
ERROR 42000: Variable 'sql_warnings' can't be set to the value of 'ÓFF'
SET @@session.sql_warnings = '¹';
ERROR 42000: Variable 'sql_warnings' can't be set to the value of '¹'
SET @@session.sql_warnings = NO;
ERROR 42000: Variable 'sql_warnings' can't be set to the value of 'NO'
'#-------------------FN_DYNVARS_166_05----------------------------#'
SET @@global.sql_warnings = 1-@@global.sql_warnings;
SELECT @@global.sql_warnings;
@@global.sql_warnings
1
SET @@global.sql_warnings = 1-@@global.sql_warnings;
SELECT @@global.sql_warnings;
@@global.sql_warnings
0
'#----------------------FN_DYNVARS_166_06------------------------#'
SELECT count(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='sql_warnings';
count(VARIABLE_VALUE)
1
'#----------------------FN_DYNVARS_166_07------------------------#'
SELECT IF(@@session.sql_warnings, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='sql_warnings';
IF(@@session.sql_warnings, "ON", "OFF") = VARIABLE_VALUE
1
SELECT @@session.sql_warnings;
@@session.sql_warnings
1
SELECT VARIABLE_VALUE
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='sql_warnings';
VARIABLE_VALUE
ON
'#---------------------FN_DYNVARS_166_08-------------------------#'
SET @@session.sql_warnings = OFF;
SELECT @@session.sql_warnings;
@@session.sql_warnings
0
SET @@session.sql_warnings = ON;
SELECT @@session.sql_warnings;
@@session.sql_warnings
1
'#---------------------FN_DYNVARS_166_09----------------------#'
SET @@session.sql_warnings = TRUE;
SELECT @@session.sql_warnings;
@@session.sql_warnings
1
SET @@session.sql_warnings = FALSE;
SELECT @@session.sql_warnings;
@@session.sql_warnings
0
SET @@session.sql_warnings = @session_start_value;
SELECT @@session.sql_warnings;
@@session.sql_warnings
0
