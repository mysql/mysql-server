SET @start_value = @@select_into_disk_sync;
SELECT @start_value;
@start_value
0
'#--------------------FN_DYNVARS_005_01------------------------#'
SET @@select_into_disk_sync = 1;
SET @@select_into_disk_sync = DEFAULT;
SELECT @@select_into_disk_sync;
@@select_into_disk_sync
0
'#--------------------FN_DYNVARS_005_02------------------------#'
SET @@select_into_disk_sync = 0;
SELECT @@select_into_disk_sync;
@@select_into_disk_sync
0
SET @@select_into_disk_sync = 1;
SELECT @@select_into_disk_sync;
@@select_into_disk_sync
1
'#--------------------FN_DYNVARS_005_03-------------------------#'
SET @@select_into_disk_sync = 2;
ERROR 42000: Variable 'select_into_disk_sync' can't be set to the value of '2'
SET @@select_into_disk_sync = -1;
ERROR 42000: Variable 'select_into_disk_sync' can't be set to the value of '-1'
SET @@select_into_disk_sync = TRUEF;
ERROR 42000: Variable 'select_into_disk_sync' can't be set to the value of 'TRUEF'
SET @@select_into_disk_sync = TRUE_F;
ERROR 42000: Variable 'select_into_disk_sync' can't be set to the value of 'TRUE_F'
SET @@select_into_disk_sync = FALSE0;
ERROR 42000: Variable 'select_into_disk_sync' can't be set to the value of 'FALSE0'
SET @@select_into_disk_sync = OON;
ERROR 42000: Variable 'select_into_disk_sync' can't be set to the value of 'OON'
SET @@select_into_disk_sync = ONN;
ERROR 42000: Variable 'select_into_disk_sync' can't be set to the value of 'ONN'
SET @@select_into_disk_sync = OOFF;
ERROR 42000: Variable 'select_into_disk_sync' can't be set to the value of 'OOFF'
SET @@select_into_disk_sync = 0FF;
ERROR 42000: Variable 'select_into_disk_sync' can't be set to the value of '0FF'
SET @@select_into_disk_sync = ' ';
ERROR 42000: Variable 'select_into_disk_sync' can't be set to the value of ' '
SET @@select_into_disk_sync = " ";
ERROR 42000: Variable 'select_into_disk_sync' can't be set to the value of ' '
SET @@select_into_disk_sync = '';
ERROR 42000: Variable 'select_into_disk_sync' can't be set to the value of ''
'#-------------------FN_DYNVARS_005_04----------------------------#'
SET @@global.select_into_disk_sync = 1-@@global.select_into_disk_sync;
SELECT @@global.select_into_disk_sync;
@@global.select_into_disk_sync
1
SET @@global.select_into_disk_sync = 1-@@global.select_into_disk_sync;
'#----------------------FN_DYNVARS_005_05------------------------#'
SELECT IF(@@select_into_disk_sync, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='select_into_disk_sync';
IF(@@select_into_disk_sync, "ON", "OFF") = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_005_06----------------------#'
SET @@select_into_disk_sync = OFF;
SELECT @@select_into_disk_sync;
@@select_into_disk_sync
0
SET @@select_into_disk_sync = ON;
SELECT @@select_into_disk_sync;
@@select_into_disk_sync
1
'#---------------------FN_DYNVARS_005_07----------------------#'
SET @@select_into_disk_sync = TRUE;
SELECT @@select_into_disk_sync;
@@select_into_disk_sync
1
SET @@select_into_disk_sync = FALSE;
SELECT @@select_into_disk_sync;
@@select_into_disk_sync
0
'#---------------------FN_DYNVARS_005_08----------------------#'
SET @@select_into_disk_sync = 0;
SELECT @@select_into_disk_sync = @@session.select_into_disk_sync;
@@select_into_disk_sync = @@session.select_into_disk_sync
1
SET @@select_into_disk_sync = 1;
SELECT @@select_into_disk_sync = @@local.select_into_disk_sync and @@local.select_into_disk_sync = @@session.select_into_disk_sync;
@@select_into_disk_sync = @@local.select_into_disk_sync and @@local.select_into_disk_sync = @@session.select_into_disk_sync
1
'#---------------------FN_DYNVARS_005_09----------------------#'
SET select_into_disk_sync = 1;
SELECT @@select_into_disk_sync;
@@select_into_disk_sync
1
SET local.select_into_disk_sync = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.select_into_disk_sync = 1' at line 1
SELECT local.select_into_disk_sync;
ERROR 42S02: Unknown table 'local' in field list
SET session.select_into_disk_sync = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'session.select_into_disk_sync = 1' at line 1
SELECT session.select_into_disk_sync;
ERROR 42S02: Unknown table 'session' in field list
select select_into_disk_sync;
ERROR 42S22: Unknown column 'select_into_disk_sync' in 'field list'
SET @@select_into_disk_sync = @start_value;
SELECT @@select_into_disk_sync;
@@select_into_disk_sync
0
