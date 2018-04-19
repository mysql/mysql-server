SET @start_read_only = @@global.read_only;
SET @start_super_read_only = @@global.super_read_only;
SELECT @start_read_only, @start_super_read_only;
@start_read_only	@start_super_read_only
0	0
'#--------------------FN_DYNVARS_139_01------------------------#'
SET @@global.super_read_only = 1;
SET @@global.super_read_only = DEFAULT;
SELECT @@global.super_read_only;
@@global.super_read_only
0
'#---------------------FN_DYNVARS_139_02-------------------------#'
SET @@global.super_read_only = @start_super_read_only;
SELECT @@global.super_read_only = 0;
@@global.super_read_only = 0
1
'#--------------------FN_DYNVARS_139_03------------------------#'
SET @@global.super_read_only = 0;
SELECT @@global.super_read_only;
@@global.super_read_only
0
SET @@global.super_read_only = 1;
SELECT @@global.super_read_only;
@@global.super_read_only
1
SET @@global.super_read_only = TRUE;
SELECT @@global.super_read_only;
@@global.super_read_only
1
SET @@global.super_read_only = FALSE;
SELECT @@global.super_read_only;
@@global.super_read_only
0
SET @@global.super_read_only = ON;
SELECT @@global.super_read_only;
@@global.super_read_only
1
SET @@global.super_read_only = OFF;
SELECT @@global.super_read_only;
@@global.super_read_only
0
'#--------------------FN_DYNVARS_139_04-------------------------#'
SET @@global.super_read_only = -1;
ERROR 42000: Variable 'super_read_only' can't be set to the value of '-1'
SELECT @@global.super_read_only;
@@global.super_read_only
0
SET @@global.super_read_only = 4294967296;
ERROR 42000: Variable 'super_read_only' can't be set to the value of '4294967296'
SELECT @@global.super_read_only;
@@global.super_read_only
0
SET @@global.super_read_only = 10240022115;
ERROR 42000: Variable 'super_read_only' can't be set to the value of '10240022115'
SELECT @@global.super_read_only;
@@global.super_read_only
0
SET @@global.super_read_only = 10000.01;
ERROR 42000: Incorrect argument type to variable 'super_read_only'
SELECT @@global.super_read_only;
@@global.super_read_only
0
SET @@global.super_read_only = -1024;
ERROR 42000: Variable 'super_read_only' can't be set to the value of '-1024'
SELECT @@global.super_read_only;
@@global.super_read_only
0
SET @@global.super_read_only = 42949672950;
ERROR 42000: Variable 'super_read_only' can't be set to the value of '42949672950'
SELECT @@global.super_read_only;
@@global.super_read_only
0
SET @@global.super_read_only = 'test';
ERROR 42000: Variable 'super_read_only' can't be set to the value of 'test'
SELECT @@global.super_read_only;
@@global.super_read_only
0
'#-------------------FN_DYNVARS_139_05----------------------------#'
SET @@session.super_read_only = 0;
ERROR HY000: Variable 'super_read_only' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@super_read_only;
@@super_read_only
0
'#----------------------FN_DYNVARS_139_06------------------------#'
SELECT IF(@@global.super_read_only, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='super_read_only';
IF(@@global.super_read_only, "ON", "OFF") = VARIABLE_VALUE
1
SELECT IF(@@read_only, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='super_read_only';
IF(@@read_only, "ON", "OFF") = VARIABLE_VALUE
0
'#---------------------FN_DYNVARS_139_07----------------------#'
SET @@global.super_read_only = 1;
SELECT @@super_read_only = @@global.super_read_only;
@@super_read_only = @@global.super_read_only
1
'#---------------------FN_DYNVARS_139_08----------------------#'
SET super_read_only = 1;
ERROR HY000: Variable 'super_read_only' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@super_read_only;
@@super_read_only
1
SET local.super_read_only = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.super_read_only = 1' at line 1
SELECT local.super_read_only;
ERROR 42S02: Unknown table 'local' in field list
SET global.super_read_only = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.super_read_only = 1' at line 1
SELECT global.super_read_only;
ERROR 42S02: Unknown table 'global' in field list
SELECT super_read_only = @@session.super_read_only;
ERROR 42S22: Unknown column 'super_read_only' in 'field list'
SET @@global.read_only = @start_read_only;
SET @@global.super_read_only = @start_super_read_only;
SELECT @@global.super_read_only;
@@global.super_read_only
0
SELECT @@global.read_only;
@@global.read_only
0
