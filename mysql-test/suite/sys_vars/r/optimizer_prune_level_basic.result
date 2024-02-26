SET @start_global_value = @@global.optimizer_prune_level;
SELECT @start_global_value;
@start_global_value
1
SET @start_session_value = @@session.optimizer_prune_level;
SELECT @start_session_value;
@start_session_value
1
'#--------------------FN_DYNVARS_115_01-------------------------#'
SET @@global.optimizer_prune_level = 0;
SET @@global.optimizer_prune_level = DEFAULT;
SELECT @@global.optimizer_prune_level;
@@global.optimizer_prune_level
1
SET @@session.optimizer_prune_level = 0;
SET @@session.optimizer_prune_level = DEFAULT;
SELECT @@session.optimizer_prune_level;
@@session.optimizer_prune_level
1
'#--------------------FN_DYNVARS_115_02-------------------------#'
SET @@global.optimizer_prune_level = DEFAULT;
SELECT @@global.optimizer_prune_level = 1;
@@global.optimizer_prune_level = 1
1
SET @@session.optimizer_prune_level = DEFAULT;
SELECT @@session.optimizer_prune_level = 1;
@@session.optimizer_prune_level = 1
1
'#--------------------FN_DYNVARS_115_03-------------------------#'
SELECT @@global.optimizer_prune_level;
@@global.optimizer_prune_level
1
SET @@global.optimizer_prune_level = 0;
SELECT @@global.optimizer_prune_level;
@@global.optimizer_prune_level
0
SET @@global.optimizer_prune_level = 1;
SELECT @@global.optimizer_prune_level;
@@global.optimizer_prune_level
1
SET @@global.optimizer_prune_level = TRUE;
SELECT @@global.optimizer_prune_level;
@@global.optimizer_prune_level
1
SET @@global.optimizer_prune_level = FALSE;
SELECT @@global.optimizer_prune_level;
@@global.optimizer_prune_level
0
'#--------------------FN_DYNVARS_115_04-------------------------#'
SELECT @@session.optimizer_prune_level;
@@session.optimizer_prune_level
1
SET @@session.optimizer_prune_level = 0;
SELECT @@session.optimizer_prune_level;
@@session.optimizer_prune_level
0
SET @@session.optimizer_prune_level = 1;
SELECT @@session.optimizer_prune_level;
@@session.optimizer_prune_level
1
SET @@session.optimizer_prune_level = TRUE;
SELECT @@session.optimizer_prune_level;
@@session.optimizer_prune_level
1
SET @@session.optimizer_prune_level = FALSE;
SELECT @@session.optimizer_prune_level;
@@session.optimizer_prune_level
0
'#------------------FN_DYNVARS_115_05-----------------------#'
SET @@global.optimizer_prune_level = ON;
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
'Bug# 34840: Since it is a boolean variable, it should not give errors on 'ON' & 'OFF' values';
SET @@global.optimizer_prune_level = OFF;
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
SET @@global.optimizer_prune_level = 'ONN';
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
SET @@global.optimizer_prune_level = "OFFF";
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
SET @@global.optimizer_prune_level = TTRUE;
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
SET @@global.optimizer_prune_level = FELSE;
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
SET @@global.optimizer_prune_level = -1024;
Warnings:
Warning	1292	Truncated incorrect optimizer_prune_level value: '-1024'
SELECT @@global.optimizer_prune_level;
@@global.optimizer_prune_level
0
'Bug# 34840: Since it is a boolean variable, it should give errors on numeric values';
SET @@global.optimizer_prune_level = 65536;
Warnings:
Warning	1292	Truncated incorrect optimizer_prune_level value: '65536'
SET @@global.optimizer_prune_level = 65530.34;
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
SET @@global.optimizer_prune_level = test;
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
SET @@session.optimizer_prune_level = ON;
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
SET @@session.optimizer_prune_level = OFF;
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
SET @@session.optimizer_prune_level = ONN;
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
SET @@session.optimizer_prune_level = ONF;
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
SET @@session.optimizer_prune_level = ON;
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
SET @@session.optimizer_prune_level = OF;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'OF' at line 1
SET @@session.optimizer_prune_level = 'OFN';
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
SET @@session.optimizer_prune_level = -2;
Warnings:
Warning	1292	Truncated incorrect optimizer_prune_level value: '-2'
SELECT @@session.optimizer_prune_level;
@@session.optimizer_prune_level
0
SET @@session.optimizer_prune_level = 65530.34;
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
SET @@session.optimizer_prune_level = 65550;
Warnings:
Warning	1292	Truncated incorrect optimizer_prune_level value: '65550'
SELECT @@session.optimizer_prune_level;
@@session.optimizer_prune_level
1
SET @@session.optimizer_prune_level = test;
ERROR 42000: Incorrect argument type to variable 'optimizer_prune_level'
'#------------------FN_DYNVARS_115_06-----------------------#'
SELECT @@global.optimizer_prune_level = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='optimizer_prune_level';
@@global.optimizer_prune_level = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_115_07-----------------------#'
SELECT @@session.optimizer_prune_level = VARIABLE_VALUE
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='optimizer_prune_level';
@@session.optimizer_prune_level = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_115_08----------------------#'
SET @@optimizer_prune_level = 0;
SET @@global.optimizer_prune_level = 1;
SELECT @@optimizer_prune_level = @@global.optimizer_prune_level;
@@optimizer_prune_level = @@global.optimizer_prune_level
0
'#---------------------FN_DYNVARS_115_09----------------------#'
SET @@optimizer_prune_level = 1;
SELECT @@optimizer_prune_level = @@local.optimizer_prune_level;
@@optimizer_prune_level = @@local.optimizer_prune_level
1
SELECT @@local.optimizer_prune_level = @@session.optimizer_prune_level;
@@local.optimizer_prune_level = @@session.optimizer_prune_level
1
'#---------------------FN_DYNVARS_115_10----------------------#'
SET optimizer_prune_level = 1;
SELECT @@optimizer_prune_level;
@@optimizer_prune_level
1
SELECT local.optimizer_prune_level;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.optimizer_prune_level;
ERROR 42S02: Unknown table 'session' in field list
SELECT optimizer_prune_level = @@session.optimizer_prune_level;
ERROR 42S22: Unknown column 'optimizer_prune_level' in 'field list'
SET @@global.optimizer_prune_level = @start_global_value;
SELECT @@global.optimizer_prune_level;
@@global.optimizer_prune_level
1
SET @@session.optimizer_prune_level = @start_session_value;
SELECT @@session.optimizer_prune_level;
@@session.optimizer_prune_level
1
