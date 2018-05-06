SET @start_global_value = @@global.completion_type;
SELECT @start_global_value;
@start_global_value
NO_CHAIN
SET @start_session_value = @@session.completion_type;
SELECT @start_session_value;
@start_session_value
NO_CHAIN
'#--------------------FN_DYNVARS_017_01-------------------------#'
SET @@global.completion_type = 2;
SET @@global.completion_type = DEFAULT;
SELECT @@global.completion_type;
@@global.completion_type
NO_CHAIN
SET @@session.completion_type = 1;
SET @@session.completion_type = DEFAULT;
SELECT @@session.completion_type;
@@session.completion_type
NO_CHAIN
'#--------------------FN_DYNVARS_017_03-------------------------#'
SET @@global.completion_type = 0;
SELECT @@global.completion_type;
@@global.completion_type
NO_CHAIN
SET @@global.completion_type = 1;
SELECT @@global.completion_type;
@@global.completion_type
CHAIN
SET @@global.completion_type = 2;
SELECT @@global.completion_type;
@@global.completion_type
RELEASE
SET @@global.completion_type = NO_CHAIN;
SELECT @@global.completion_type;
@@global.completion_type
NO_CHAIN
SET @@global.completion_type = CHAIN;
SELECT @@global.completion_type;
@@global.completion_type
CHAIN
SET @@global.completion_type = 'RELEASE';
SELECT @@global.completion_type;
@@global.completion_type
RELEASE
'#--------------------FN_DYNVARS_017_04-------------------------#'
SET @@session.completion_type = 0;
SELECT @@session.completion_type;
@@session.completion_type
NO_CHAIN
SET @@session.completion_type = 1;
SELECT @@session.completion_type;
@@session.completion_type
CHAIN
SET @@session.completion_type = 2;
SELECT @@session.completion_type;
@@session.completion_type
RELEASE
SET @@session.completion_type = 'NO_CHAIN';
SELECT @@session.completion_type;
@@session.completion_type
NO_CHAIN
SET @@session.completion_type = 'CHAIN';
SELECT @@session.completion_type;
@@session.completion_type
CHAIN
SET @@session.completion_type = 'RELEASE';
SELECT @@session.completion_type;
@@session.completion_type
RELEASE
'#------------------FN_DYNVARS_017_05-----------------------#'
set sql_mode=TRADITIONAL;
SET @@global.completion_type = 10;
ERROR 42000: Variable 'completion_type' can't be set to the value of '10'
SET @@global.completion_type = -1024;
ERROR 42000: Variable 'completion_type' can't be set to the value of '-1024'
SET @@global.completion_type = 2.4;
ERROR 42000: Incorrect argument type to variable 'completion_type'
SET @@global.completion_type = OFF;
ERROR 42000: Variable 'completion_type' can't be set to the value of 'OFF'
SET @@session.completion_type = 10;
ERROR 42000: Variable 'completion_type' can't be set to the value of '10'
SET @@session.completion_type = -2;
ERROR 42000: Variable 'completion_type' can't be set to the value of '-2'
SET @@session.completion_type = 1.2;
ERROR 42000: Incorrect argument type to variable 'completion_type'
SET @@session.completion_type = ON;
ERROR 42000: Variable 'completion_type' can't be set to the value of 'ON'
'#------------------FN_DYNVARS_017_06-----------------------#'
SELECT @@global.completion_type = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='completion_type';
@@global.completion_type = VARIABLE_VALUE
1
SELECT @@session.completion_type = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='completion_type';
@@session.completion_type = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_017_07-----------------------#'
SET @@global.completion_type = TRUE;
SELECT @@global.completion_type;
@@global.completion_type
CHAIN
SET @@global.completion_type = FALSE;
SELECT @@global.completion_type;
@@global.completion_type
NO_CHAIN
'#---------------------FN_DYNVARS_001_08----------------------#'
SET @@completion_type = 1;
SELECT @@completion_type = @@local.completion_type;
@@completion_type = @@local.completion_type
1
SELECT @@local.completion_type = @@session.completion_type;
@@local.completion_type = @@session.completion_type
1
'#---------------------FN_DYNVARS_001_09----------------------#'
SET completion_type = 1;
SELECT @@completion_type;
@@completion_type
CHAIN
SET local.completion_type = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.completion_type = 1' at line 1
SELECT local.completion_type;
ERROR 42S02: Unknown table 'local' in field list
SET session.completion_type = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'session.completion_type = 1' at line 1
SELECT session.completion_type;
ERROR 42S02: Unknown table 'session' in field list
SELECT completion_type = @@session.completion_type;
ERROR 42S22: Unknown column 'completion_type' in 'field list'
SET @@global.completion_type = @start_global_value;
SELECT @@global.completion_type;
@@global.completion_type
NO_CHAIN
SET @@session.completion_type = @start_session_value;
SELECT @@session.completion_type;
@@session.completion_type
NO_CHAIN
set sql_mode='';
