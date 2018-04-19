SET @start_value = @@global.binlog_order_commits;
SELECT @start_value;
@start_value
1
'#---------------------FN_DYNVARS_004_01-------------------------#'
SET @@global.binlog_order_commits = DEFAULT;
SELECT @@global.binlog_order_commits = FALSE;
@@global.binlog_order_commits = FALSE
0
'#--------------------FN_DYNVARS_004_02------------------------#'
SET @@global.binlog_order_commits = ON;
SELECT @@global.binlog_order_commits;
@@global.binlog_order_commits
1
SET @@global.binlog_order_commits = OFF;
SELECT @@global.binlog_order_commits;
@@global.binlog_order_commits
0
'#--------------------FN_DYNVARS_004_03-------------------------#'
SET @@global.binlog_order_commits = 2;
ERROR 42000: Variable 'binlog_order_commits' can't be set to the value of '2'
SET @@global.binlog_order_commits = -1;
ERROR 42000: Variable 'binlog_order_commits' can't be set to the value of '-1'
SET @@global.binlog_order_commits = TRUEF;
ERROR 42000: Variable 'binlog_order_commits' can't be set to the value of 'TRUEF'
SET @@global.binlog_order_commits = TRUE_F;
ERROR 42000: Variable 'binlog_order_commits' can't be set to the value of 'TRUE_F'
SET @@global.binlog_order_commits = FALSE0;
ERROR 42000: Variable 'binlog_order_commits' can't be set to the value of 'FALSE0'
SET @@global.binlog_order_commits = OON;
ERROR 42000: Variable 'binlog_order_commits' can't be set to the value of 'OON'
SET @@global.binlog_order_commits = ONN;
ERROR 42000: Variable 'binlog_order_commits' can't be set to the value of 'ONN'
SET @@global.binlog_order_commits = OOFF;
ERROR 42000: Variable 'binlog_order_commits' can't be set to the value of 'OOFF'
SET @@global.binlog_order_commits = 0FF;
ERROR 42000: Variable 'binlog_order_commits' can't be set to the value of '0FF'
SET @@global.binlog_order_commits = ' ';
ERROR 42000: Variable 'binlog_order_commits' can't be set to the value of ' '
SET @@global.binlog_order_commits = " ";
ERROR 42000: Variable 'binlog_order_commits' can't be set to the value of ' '
SET @@global.binlog_order_commits = '';
ERROR 42000: Variable 'binlog_order_commits' can't be set to the value of ''
'#-------------------FN_DYNVARS_004_04----------------------------#'
SET @@session.binlog_order_commits = OFF;
ERROR HY000: Variable 'binlog_order_commits' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@session.binlog_order_commits;
ERROR HY000: Variable 'binlog_order_commits' is a GLOBAL variable
'#----------------------FN_DYNVARS_004_05------------------------#'
SELECT IF(@@global.binlog_order_commits, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='binlog_order_commits';
IF(@@global.binlog_order_commits, "ON", "OFF") = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_004_06----------------------#'
SET @@global.binlog_order_commits = 0;
SELECT @@global.binlog_order_commits;
@@global.binlog_order_commits
0
SET @@global.binlog_order_commits = 1;
SELECT @@global.binlog_order_commits;
@@global.binlog_order_commits
1
'#---------------------FN_DYNVARS_004_07----------------------#'
SET @@global.binlog_order_commits = TRUE;
SELECT @@global.binlog_order_commits;
@@global.binlog_order_commits
1
SET @@global.binlog_order_commits = FALSE;
SELECT @@global.binlog_order_commits;
@@global.binlog_order_commits
0
'#---------------------FN_DYNVARS_004_08----------------------#'
SET @@global.binlog_order_commits = ON;
SELECT @@binlog_order_commits = @@global.binlog_order_commits;
@@binlog_order_commits = @@global.binlog_order_commits
1
'#---------------------FN_DYNVARS_004_09----------------------#'
SET binlog_order_commits = ON;
ERROR HY000: Variable 'binlog_order_commits' is a GLOBAL variable and should be set with SET GLOBAL
SET local.binlog_order_commits = OFF;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.binlog_order_commits = OFF' at line 1
SELECT local.binlog_order_commits;
ERROR 42S02: Unknown table 'local' in field list
SET global.binlog_order_commits = ON;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.binlog_order_commits = ON' at line 1
SELECT global.binlog_order_commits;
ERROR 42S02: Unknown table 'global' in field list
SELECT binlog_order_commits = @@session.binlog_order_commits;
ERROR 42S22: Unknown column 'binlog_order_commits' in 'field list'
SET @@global.binlog_order_commits = @start_value;
SELECT @@global.binlog_order_commits;
@@global.binlog_order_commits
1
