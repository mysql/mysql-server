include/only_with_option.inc [GLOBAL.xa_detach_on_prepare = 1]
SET @session_start_value = @@session.pseudo_replica_mode;
SELECT @session_start_value;
@session_start_value
0
'#--------------------FN_DYNVARS_156_01------------------------#'
SET @@session.pseudo_replica_mode = 0;
Warnings:
Warning	1231	'pseudo_replica_mode' change was ineffective.
SET @@session.pseudo_replica_mode = DEFAULT;
Warnings:
Warning	1231	'pseudo_replica_mode' change was ineffective.
SELECT @@session.pseudo_replica_mode;
@@session.pseudo_replica_mode
0
SET @@session.pseudo_replica_mode = 1;
SET @@session.pseudo_replica_mode = DEFAULT;
Warnings:
Warning	1231	Slave applier execution mode not active, statement ineffective.
SELECT @@session.pseudo_replica_mode;
@@session.pseudo_replica_mode
0
SET @@session.pseudo_replica_mode = 1;
SET @@session.pseudo_slave_mode = DEFAULT;
Warnings:
Warning	1287	'@@pseudo_slave_mode' is deprecated and will be removed in a future release. Please use pseudo_replica_mode instead.
Warning	1231	Slave applier execution mode not active, statement ineffective.
SELECT @@session.pseudo_replica_mode;
@@session.pseudo_replica_mode
0
'#---------------------FN_DYNVARS_156_02-------------------------#'
SET pseudo_replica_mode = 1;
SELECT @@pseudo_replica_mode;
@@pseudo_replica_mode
1
SELECT session.pseudo_replica_mode;
ERROR 42S02: Unknown table 'session' in field list
SELECT local.pseudo_replica_mode;
ERROR 42S02: Unknown table 'local' in field list
SET session pseudo_replica_mode = 0;
Warnings:
Warning	1231	Slave applier execution mode not active, statement ineffective.
SELECT @@session.pseudo_replica_mode;
@@session.pseudo_replica_mode
0
'#--------------------FN_DYNVARS_156_03------------------------#'
SET @@session.pseudo_replica_mode = 0;
Warnings:
Warning	1231	'pseudo_replica_mode' change was ineffective.
SELECT @@session.pseudo_replica_mode;
@@session.pseudo_replica_mode
0
SET @@session.pseudo_replica_mode = 1;
SELECT @@session.pseudo_replica_mode;
@@session.pseudo_replica_mode
1
'#--------------------FN_DYNVARS_156_04-------------------------#'
SET @@session.pseudo_replica_mode = -1;
ERROR 42000: Variable 'pseudo_replica_mode' can't be set to the value of '-1'
SET @@session.pseudo_replica_mode = 2;
ERROR 42000: Variable 'pseudo_replica_mode' can't be set to the value of '2'
SET @@session.pseudo_replica_mode = "T";
ERROR 42000: Variable 'pseudo_replica_mode' can't be set to the value of 'T'
SET @@session.pseudo_replica_mode = "Y";
ERROR 42000: Variable 'pseudo_replica_mode' can't be set to the value of 'Y'
SET @@session.pseudo_replica_mode = TRÜE;
ERROR 42000: Variable 'pseudo_replica_mode' can't be set to the value of 'TRÜE'
SET @@session.pseudo_replica_mode = ÕN;
ERROR 42000: Variable 'pseudo_replica_mode' can't be set to the value of 'ÕN'
SET @@session.pseudo_replica_mode = OF;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'OF' at line 1
SET @@session.pseudo_replica_mode = ÓFF;
ERROR 42000: Variable 'pseudo_replica_mode' can't be set to the value of 'ÓFF'
SET @@session.pseudo_replica_mode = '¹';
ERROR 42000: Variable 'pseudo_replica_mode' can't be set to the value of '¹'
SET @@session.pseudo_replica_mode = NO;
ERROR 42000: Variable 'pseudo_replica_mode' can't be set to the value of 'NO'
'#-------------------FN_DYNVARS_156_05----------------------------#'
SET @@global.pseudo_replica_mode = 0;
ERROR HY000: Variable 'pseudo_replica_mode' is a SESSION variable and can't be used with SET GLOBAL
SELECT @@global.pseudo_replica_mode;
ERROR HY000: Variable 'pseudo_replica_mode' is a SESSION variable
'#----------------------FN_DYNVARS_156_06------------------------#'
SELECT count(VARIABLE_VALUE) FROM performance_schema.session_variables WHERE VARIABLE_NAME='pseudo_replica_mode';
count(VARIABLE_VALUE)
1
'#----------------------FN_DYNVARS_156_07------------------------#'
SELECT IF(@@session.pseudo_replica_mode, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='pseudo_replica_mode';
IF(@@session.pseudo_replica_mode, "ON", "OFF") = VARIABLE_VALUE
1
SELECT @@session.pseudo_replica_mode;
@@session.pseudo_replica_mode
1
SELECT VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='pseudo_replica_mode';
VARIABLE_VALUE
ON
'#---------------------FN_DYNVARS_156_08-------------------------#'
SET @@session.pseudo_replica_mode = OFF;
Warnings:
Warning	1231	Slave applier execution mode not active, statement ineffective.
SELECT @@session.pseudo_replica_mode;
@@session.pseudo_replica_mode
0
SET @@session.pseudo_replica_mode = ON;
SELECT @@session.pseudo_replica_mode;
@@session.pseudo_replica_mode
1
'#---------------------FN_DYNVARS_156_09----------------------#'
SET @@session.pseudo_replica_mode = TRUE;
SELECT @@session.pseudo_replica_mode;
@@session.pseudo_replica_mode
1
SET @@session.pseudo_replica_mode = FALSE;
Warnings:
Warning	1231	Slave applier execution mode not active, statement ineffective.
SELECT @@session.pseudo_replica_mode;
@@session.pseudo_replica_mode
0
'#---------------------FN_DYNVARS_156_10----------------------#'
BEGIN;
SET @@session.pseudo_replica_mode = TRUE;
ERROR HY000: The system variable pseudo_replica_mode cannot be set when there is an ongoing transaction.
SET @@session.pseudo_replica_mode = FALSE;
ERROR HY000: The system variable pseudo_replica_mode cannot be set when there is an ongoing transaction.
COMMIT;
XA START 'xa1';
SET @@session.pseudo_replica_mode = TRUE;
ERROR HY000: The system variable pseudo_replica_mode cannot be set when there is an ongoing transaction.
SET @@session.pseudo_replica_mode = FALSE;
ERROR HY000: The system variable pseudo_replica_mode cannot be set when there is an ongoing transaction.
XA END 'xa1';
SET @@session.pseudo_replica_mode = TRUE;
ERROR HY000: The system variable pseudo_replica_mode cannot be set when there is an ongoing transaction.
SET @@session.pseudo_replica_mode = FALSE;
ERROR HY000: The system variable pseudo_replica_mode cannot be set when there is an ongoing transaction.
XA PREPARE 'xa1';
SET @@session.pseudo_replica_mode = TRUE;
SET @@session.pseudo_replica_mode = FALSE;
Warnings:
Warning	1231	Slave applier execution mode not active, statement ineffective.
XA COMMIT 'xa1';
SET @@session.pseudo_replica_mode = TRUE;
SET @@session.pseudo_replica_mode = FALSE;
Warnings:
Warning	1231	Slave applier execution mode not active, statement ineffective.
SET @@session.pseudo_replica_mode = @session_start_value;
Warnings:
Warning	1231	'pseudo_replica_mode' change was ineffective.
SELECT @@session.pseudo_replica_mode;
@@session.pseudo_replica_mode
0
