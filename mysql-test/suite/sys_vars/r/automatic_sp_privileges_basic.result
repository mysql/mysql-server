SET @start_value = @@global.automatic_sp_privileges;
SELECT @start_value;
@start_value
1
'#--------------------FN_DYNVARS_004_01------------------------#'
SET @@global.automatic_sp_privileges = 0;
SET @@global.automatic_sp_privileges = DEFAULT;
SELECT @@global.automatic_sp_privileges;
@@global.automatic_sp_privileges
1
'#---------------------FN_DYNVARS_004_02-------------------------#'
SET @@global.automatic_sp_privileges = @start_value;
SELECT @@global.automatic_sp_privileges = TRUE;
@@global.automatic_sp_privileges = TRUE
1
'#--------------------FN_DYNVARS_004_03------------------------#'
SET @@global.automatic_sp_privileges = 0;
SELECT @@global.automatic_sp_privileges;
@@global.automatic_sp_privileges
0
SET @@global.automatic_sp_privileges = 1;
SELECT @@global.automatic_sp_privileges;
@@global.automatic_sp_privileges
1
'#--------------------FN_DYNVARS_004_04-------------------------#'
SET @@global.automatic_sp_privileges = 2;
ERROR 42000: Variable 'automatic_sp_privileges' can't be set to the value of '2'
SET @@global.automatic_sp_privileges = -1;
ERROR 42000: Variable 'automatic_sp_privileges' can't be set to the value of '-1'
SET @@global.automatic_sp_privileges = TRUEF;
ERROR 42000: Variable 'automatic_sp_privileges' can't be set to the value of 'TRUEF'
SET @@global.automatic_sp_privileges = TRUE_F;
ERROR 42000: Variable 'automatic_sp_privileges' can't be set to the value of 'TRUE_F'
SET @@global.automatic_sp_privileges = FALSE0;
ERROR 42000: Variable 'automatic_sp_privileges' can't be set to the value of 'FALSE0'
SET @@global.automatic_sp_privileges = OON;
ERROR 42000: Variable 'automatic_sp_privileges' can't be set to the value of 'OON'
SET @@global.automatic_sp_privileges = ONN;
ERROR 42000: Variable 'automatic_sp_privileges' can't be set to the value of 'ONN'
SET @@global.automatic_sp_privileges = OOFF;
ERROR 42000: Variable 'automatic_sp_privileges' can't be set to the value of 'OOFF'
SET @@global.automatic_sp_privileges = 0FF;
ERROR 42000: Variable 'automatic_sp_privileges' can't be set to the value of '0FF'
SET @@global.automatic_sp_privileges = ' ';
ERROR 42000: Variable 'automatic_sp_privileges' can't be set to the value of ' '
SET @@global.automatic_sp_privileges = " ";
ERROR 42000: Variable 'automatic_sp_privileges' can't be set to the value of ' '
SET @@global.automatic_sp_privileges = '';
ERROR 42000: Variable 'automatic_sp_privileges' can't be set to the value of ''
'#-------------------FN_DYNVARS_004_05----------------------------#'
SET @@session.automatic_sp_privileges = 1;
ERROR HY000: Variable 'automatic_sp_privileges' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@session.automatic_sp_privileges;
ERROR HY000: Variable 'automatic_sp_privileges' is a GLOBAL variable
'#----------------------FN_DYNVARS_004_06------------------------#'
SELECT IF(@@global.automatic_sp_privileges, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='automatic_sp_privileges';
IF(@@global.automatic_sp_privileges, "ON", "OFF") = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_004_07----------------------#'
SET @@global.automatic_sp_privileges = OFF;
SELECT @@global.automatic_sp_privileges;
@@global.automatic_sp_privileges
0
SET @@global.automatic_sp_privileges = ON;
SELECT @@global.automatic_sp_privileges;
@@global.automatic_sp_privileges
1
'#---------------------FN_DYNVARS_004_08----------------------#'
SET @@global.automatic_sp_privileges = TRUE;
SELECT @@global.automatic_sp_privileges;
@@global.automatic_sp_privileges
1
SET @@global.automatic_sp_privileges = FALSE;
SELECT @@global.automatic_sp_privileges;
@@global.automatic_sp_privileges
0
'#---------------------FN_DYNVARS_004_09----------------------#'
SET @@global.automatic_sp_privileges = 1;
SELECT @@automatic_sp_privileges = @@global.automatic_sp_privileges;
@@automatic_sp_privileges = @@global.automatic_sp_privileges
1
'#---------------------FN_DYNVARS_004_10----------------------#'
SET automatic_sp_privileges = 1;
ERROR HY000: Variable 'automatic_sp_privileges' is a GLOBAL variable and should be set with SET GLOBAL
SET local.automatic_sp_privileges = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.automatic_sp_privileges = 1' at line 1
SELECT local.automatic_sp_privileges;
ERROR 42S02: Unknown table 'local' in field list
SET global.automatic_sp_privileges = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.automatic_sp_privileges = 1' at line 1
SELECT global.automatic_sp_privileges;
ERROR 42S02: Unknown table 'global' in field list
SELECT automatic_sp_privileges = @@session.automatic_sp_privileges;
ERROR 42S22: Unknown column 'automatic_sp_privileges' in 'field list'
SET @@global.automatic_sp_privileges = @start_value;
SELECT @@global.automatic_sp_privileges;
@@global.automatic_sp_privileges
1
