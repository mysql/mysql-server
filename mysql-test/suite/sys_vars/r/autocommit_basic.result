SET @start_value = @@global.autocommit;
SELECT @start_value;
@start_value
1
'#--------------------FN_DYNVARS_003_01------------------------#'
SET @@autocommit = 0;
SET @@autocommit = DEFAULT;
SELECT @@autocommit;
@@autocommit
1
'#---------------------FN_DYNVARS_003_02-------------------------#'
SET @@autocommit = @start_value;
SELECT @@autocommit = 1;
@@autocommit = 1
1
'#--------------------FN_DYNVARS_003_03------------------------#'
SET @@autocommit = 0;
SELECT @@autocommit;
@@autocommit
0
SET @@autocommit = 1;
SELECT @@autocommit;
@@autocommit
1
'#--------------------FN_DYNVARS_003_04-------------------------#'
SET @@autocommit = 2;
ERROR 42000: Variable 'autocommit' can't be set to the value of '2'
SET @@autocommit = -1;
ERROR 42000: Variable 'autocommit' can't be set to the value of '-1'
SET @@autocommit = TRUEF;
ERROR 42000: Variable 'autocommit' can't be set to the value of 'TRUEF'
SET @@autocommit = TRUE_F;
ERROR 42000: Variable 'autocommit' can't be set to the value of 'TRUE_F'
SET @@autocommit = FALSE0;
ERROR 42000: Variable 'autocommit' can't be set to the value of 'FALSE0'
SET @@autocommit = OON;
ERROR 42000: Variable 'autocommit' can't be set to the value of 'OON'
SET @@autocommit = ONN;
ERROR 42000: Variable 'autocommit' can't be set to the value of 'ONN'
SET @@autocommit = OOFF;
ERROR 42000: Variable 'autocommit' can't be set to the value of 'OOFF'
SET @@autocommit = 0FF;
ERROR 42000: Variable 'autocommit' can't be set to the value of '0FF'
SET @@autocommit = ' ';
ERROR 42000: Variable 'autocommit' can't be set to the value of ' '
SET @@autocommit = " ";
ERROR 42000: Variable 'autocommit' can't be set to the value of ' '
SET @@autocommit = '';
ERROR 42000: Variable 'autocommit' can't be set to the value of ''
'#-------------------FN_DYNVARS_003_05----------------------------#'
SET @@global.autocommit = 0;
SELECT @@global.autocommit;
@@global.autocommit
0
SET @@global.autocommit = 1;
'#----------------------FN_DYNVARS_003_06------------------------#'
SELECT IF(@@session.autocommit, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='autocommit';
IF(@@session.autocommit, "ON", "OFF") = VARIABLE_VALUE
1
'#----------------------FN_DYNVARS_003_07------------------------#'
SET @@autocommit = 1;
SELECT IF(@@autocommit, "ON", "OFF") = VARIABLE_VALUE
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='autocommit';
IF(@@autocommit, "ON", "OFF") = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_003_08-------------------------#'
SET @@autocommit = OFF;
SELECT @@autocommit;
@@autocommit
0
SET @@autocommit = ON;
SELECT @@autocommit;
@@autocommit
1
'#---------------------FN_DYNVARS_003_09----------------------#'
SET @@autocommit = TRUE;
SELECT @@autocommit;
@@autocommit
1
SET @@autocommit = FALSE;
SELECT @@autocommit;
@@autocommit
0
'#---------------------FN_DYNVARS_003_10----------------------#'
SET @@autocommit = 0;
SELECT @@autocommit = @@local.autocommit;
@@autocommit = @@local.autocommit
1
SELECT @@local.autocommit = @@session.autocommit;
@@local.autocommit = @@session.autocommit
1
SET @@autocommit = 1;
SELECT @@autocommit = @@local.autocommit;
@@autocommit = @@local.autocommit
1
SELECT @@session.autocommit = @@autocommit;
@@session.autocommit = @@autocommit
1
'#---------------------FN_DYNVARS_003_11----------------------#'
SET autocommit = 1;
SELECT @@autocommit;
@@autocommit
1
SELECT local.autocommit;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.autocommit;
ERROR 42S02: Unknown table 'session' in field list
SELECT autocommit = @@session.autocommit;
ERROR 42S22: Unknown column 'autocommit' in 'field list'
SET @@global.autocommit = @start_value;
SELECT @@global.autocommit;
@@global.autocommit
1
