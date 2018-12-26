SET @start_value = @@session.identity;
SELECT @start_value;
@start_value
0
'#--------------------FN_DYNVARS_035_01------------------------#'
SET @@session.identity = 99;
'Variable is giving error on assigning Default value';
SELECT @@session.identity;
@@session.identity
99
'#---------------------FN_DYNVARS_035_02-------------------------#'
SET @@session.identity = @start_value;
SELECT @@session.identity = 0;
@@session.identity = 0
1
'#--------------------FN_DYNVARS_035_03------------------------#'
SET @@session.identity = 0;
SELECT @@session.identity;
@@session.identity
0
SET @@session.identity = 1099;
SELECT @@session.identity;
@@session.identity
1099
SET @@session.identity = 1800;
SELECT @@session.identity;
@@session.identity
1800
SET @@session.identity = 65535;
SELECT @@session.identity;
@@session.identity
65535
'#--------------------FN_DYNVARS_035_04-------------------------#'
SET @@session.identity = -1;
Warnings:
Warning	1292	Truncated incorrect identity value: '-1'
SELECT @@session.identity;
@@session.identity
0
SET @@session.identity = 100000000000;
SELECT @@session.identity;
@@session.identity
100000000000
SET @@session.identity = 10000.01;
ERROR 42000: Incorrect argument type to variable 'identity'
SELECT @@session.identity;
@@session.identity
100000000000
SET @@session.identity = -1024;
Warnings:
Warning	1292	Truncated incorrect identity value: '-1024'
SELECT @@session.identity;
@@session.identity
0
SET @@session.identity = 42949672950;
SELECT @@session.identity;
@@session.identity
42949672950
SET @@session.identity = " ";
ERROR 42000: Incorrect argument type to variable 'identity'
SELECT @@session.identity;
@@session.identity
42949672950
SET @@session.identity = ' ';
ERROR 42000: Incorrect argument type to variable 'identity'
SELECT @@session.identity;
@@session.identity
42949672950
SET @@session.identity = ON;
ERROR 42000: Incorrect argument type to variable 'identity'
SELECT @@session.identity;
@@session.identity
42949672950
SET @@session.identity = 'test';
ERROR 42000: Incorrect argument type to variable 'identity'
SELECT @@session.identity;
@@session.identity
42949672950
'#-------------------FN_DYNVARS_035_05----------------------------#'
SET @@global.identity = 0;
ERROR HY000: Variable 'identity' is a SESSION variable and can't be used with SET GLOBAL
SELECT @@global.identity;
ERROR HY000: Variable 'identity' is a SESSION variable
'#----------------------FN_DYNVARS_035_06------------------------#'
SELECT @@global.identity = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='identity';
ERROR HY000: Variable 'identity' is a SESSION variable
SELECT @@session.identity = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='identity';
@@session.identity = VARIABLE_VALUE
1
'#-------------------FN_DYNVARS_035_07----------------------------#'
SET @@global.identity = 0;
ERROR HY000: Variable 'identity' is a SESSION variable and can't be used with SET GLOBAL
SELECT @@global.identity;
ERROR HY000: Variable 'identity' is a SESSION variable
'#---------------------FN_DYNVARS_035_08----------------------#'
SET @@session.identity = TRUE;
SELECT @@session.identity;
@@session.identity
1
SET @@session.identity = FALSE;
SELECT @@session.identity;
@@session.identity
0
'#---------------------FN_DYNVARS_035_09----------------------#'
SET @@session.identity = 1;
SELECT @@identity = @@session.identity;
@@identity = @@session.identity
1
SELECT @@session.identity = @@local.identity;
@@session.identity = @@local.identity
1
'#---------------------FN_DYNVARS_035_10----------------------#'
SET identity = 1;
SELECT @@identity;
@@identity
1
SET local.identity = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.identity = 1' at line 1
SELECT local.identity;
ERROR 42S02: Unknown table 'local' in field list
SET session.identity = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'session.identity = 1' at line 1
SELECT session.identity;
ERROR 42S02: Unknown table 'session' in field list
SELECT identity = @@session.identity;
ERROR 42S22: Unknown column 'identity' in 'field list'
SET @@session.identity = @start_value;
SELECT @@session.identity;
@@session.identity
0
