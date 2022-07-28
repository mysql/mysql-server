SET @start_value = @@global.event_scheduler;
SELECT @start_value;
@start_value
ON
'#---------------------FN_DYNVARS_004_01-------------------------#'
SET @@global.event_scheduler = DEFAULT;
SELECT @@global.event_scheduler;
@@global.event_scheduler
ON
'#--------------------FN_DYNVARS_004_02------------------------#'
SET @@global.event_scheduler = ON;
SELECT @@global.event_scheduler;
@@global.event_scheduler
ON
SET @@global.event_scheduler = OFF;
SELECT @@global.event_scheduler;
@@global.event_scheduler
OFF
SET @@global.event_scheduler = DISABLED;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of 'DISABLED'
SELECT @@global.event_scheduler;
@@global.event_scheduler
OFF
'#--------------------FN_DYNVARS_004_03-------------------------#'
SET @@global.event_scheduler = 2;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of '2'
SET @@global.event_scheduler = -1;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of '-1'
SET @@global.event_scheduler = TRUEF;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of 'TRUEF'
SET @@global.event_scheduler = TRUE_F;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of 'TRUE_F'
SET @@global.event_scheduler = FALSE0;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of 'FALSE0'
SET @@global.event_scheduler = OON;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of 'OON'
SET @@global.event_scheduler = ONN;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of 'ONN'
SET @@global.event_scheduler = OOFF;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of 'OOFF'
SET @@global.event_scheduler = 0FF;
ERROR 42000: Variable 'event_scheduler' can't be set to the value of '0FF'
SET @@global.event_scheduler = ' ';
ERROR 42000: Variable 'event_scheduler' can't be set to the value of ' '
SET @@global.event_scheduler = " ";
ERROR 42000: Variable 'event_scheduler' can't be set to the value of ' '
SET @@global.event_scheduler = '';
ERROR 42000: Variable 'event_scheduler' can't be set to the value of ''
'#-------------------FN_DYNVARS_004_04----------------------------#'
SET @@session.event_scheduler = OFF;
ERROR HY000: Variable 'event_scheduler' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@session.event_scheduler;
ERROR HY000: Variable 'event_scheduler' is a GLOBAL variable
'#----------------------FN_DYNVARS_004_05------------------------#'
SELECT @@global.event_scheduler = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='event_scheduler';
@@global.event_scheduler = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_004_06----------------------#'
SET @@global.event_scheduler = 0;
SELECT @@global.event_scheduler;
@@global.event_scheduler
OFF
SET @@global.event_scheduler = 1;
SELECT @@global.event_scheduler;
@@global.event_scheduler
ON
'#---------------------FN_DYNVARS_004_07----------------------#'
SET @@global.event_scheduler = TRUE;
SELECT @@global.event_scheduler;
@@global.event_scheduler
ON
SET @@global.event_scheduler = FALSE;
SELECT @@global.event_scheduler;
@@global.event_scheduler
OFF
'#---------------------FN_DYNVARS_004_08----------------------#'
SET @@global.event_scheduler = ON;
SELECT @@event_scheduler = @@global.event_scheduler;
@@event_scheduler = @@global.event_scheduler
1
'#---------------------FN_DYNVARS_004_09----------------------#'
SET event_scheduler = ON;
ERROR HY000: Variable 'event_scheduler' is a GLOBAL variable and should be set with SET GLOBAL
SET local.event_scheduler = OFF;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.event_scheduler = OFF' at line 1
SELECT local.event_scheduler;
ERROR 42S02: Unknown table 'local' in field list
SET global.event_scheduler = ON;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.event_scheduler = ON' at line 1
SELECT global.event_scheduler;
ERROR 42S02: Unknown table 'global' in field list
SELECT event_scheduler;
ERROR 42S22: Unknown column 'event_scheduler' in 'field list'
SELECT @@session.event_scheduler;
ERROR HY000: Variable 'event_scheduler' is a GLOBAL variable
SET @@global.event_scheduler = @start_value;
SELECT @@global.event_scheduler;
@@global.event_scheduler
ON
# restart
