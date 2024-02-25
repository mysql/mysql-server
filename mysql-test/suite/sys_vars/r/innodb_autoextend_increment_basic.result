SET @global_start_value = @@global.innodb_autoextend_increment ;
'#--------------------FN_DYNVARS_046_01------------------------#'
SET @@global.innodb_autoextend_increment  = 0;
Warnings:
Warning	1292	Truncated incorrect innodb_autoextend_increment value: '0'
SET @@global.innodb_autoextend_increment  = DEFAULT;
SELECT @@global.innodb_autoextend_increment ;
@@global.innodb_autoextend_increment
64
'#---------------------FN_DYNVARS_046_02-------------------------#'
SET innodb_autoextend_increment  = 1;
ERROR HY000: Variable 'innodb_autoextend_increment' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@innodb_autoextend_increment ;
@@innodb_autoextend_increment
64
SELECT local.innodb_autoextend_increment ;
ERROR 42S02: Unknown table 'local' in field list
SET global innodb_autoextend_increment  = 0;
Warnings:
Warning	1292	Truncated incorrect innodb_autoextend_increment value: '0'
SELECT @@global.innodb_autoextend_increment ;
@@global.innodb_autoextend_increment
1
'#--------------------FN_DYNVARS_046_03------------------------#'
SET @@global.innodb_autoextend_increment  = 1;
SELECT @@global.innodb_autoextend_increment ;
@@global.innodb_autoextend_increment
1
SET @@global.innodb_autoextend_increment  = 1000;
SELECT @@global.innodb_autoextend_increment ;
@@global.innodb_autoextend_increment
1000
'#--------------------FN_DYNVARS_046_04-------------------------#'
SET @@global.innodb_autoextend_increment  = -1;
Warnings:
Warning	1292	Truncated incorrect innodb_autoextend_increment value: '-1'
SELECT @@global.innodb_autoextend_increment;
@@global.innodb_autoextend_increment
1
SET @@global.innodb_autoextend_increment  = "T";
ERROR 42000: Incorrect argument type to variable 'innodb_autoextend_increment'
SELECT @@global.innodb_autoextend_increment;
@@global.innodb_autoextend_increment
1
SET @@global.innodb_autoextend_increment  = "Y";
ERROR 42000: Incorrect argument type to variable 'innodb_autoextend_increment'
SELECT @@global.innodb_autoextend_increment;
@@global.innodb_autoextend_increment
1
SET @@global.innodb_autoextend_increment  = 1001;
Warnings:
Warning	1292	Truncated incorrect innodb_autoextend_increment value: '1001'
SELECT @@global.innodb_autoextend_increment;
@@global.innodb_autoextend_increment
1000
SET @@global.innodb_autoextend_increment  = 2000 ;
Warnings:
Warning	1292	Truncated incorrect innodb_autoextend_increment value: '2000'
SELECT @@global.innodb_autoextend_increment;
@@global.innodb_autoextend_increment
1000
'#----------------------FN_DYNVARS_046_05------------------------#'
SELECT @@global.innodb_autoextend_increment  = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_autoextend_increment';
@@global.innodb_autoextend_increment  = VARIABLE_VALUE
1
SELECT @@global.innodb_autoextend_increment ;
@@global.innodb_autoextend_increment
1000
SELECT VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='innodb_autoextend_increment';
VARIABLE_VALUE
1000
'#---------------------FN_DYNVARS_046_06-------------------------#'
SET @@global.innodb_autoextend_increment  = OFF;
ERROR 42000: Incorrect argument type to variable 'innodb_autoextend_increment'
SELECT @@global.innodb_autoextend_increment ;
@@global.innodb_autoextend_increment
1000
SET @@global.innodb_autoextend_increment  = ON;
ERROR 42000: Incorrect argument type to variable 'innodb_autoextend_increment'
SELECT @@global.innodb_autoextend_increment ;
@@global.innodb_autoextend_increment
1000
'#---------------------FN_DYNVARS_046_07----------------------#'
SET @@global.innodb_autoextend_increment  = TRUE;
SELECT @@global.innodb_autoextend_increment ;
@@global.innodb_autoextend_increment
1
SET @@global.innodb_autoextend_increment  = FALSE;
Warnings:
Warning	1292	Truncated incorrect innodb_autoextend_increment value: '0'
SELECT @@global.innodb_autoextend_increment ;
@@global.innodb_autoextend_increment
1
SET @@global.innodb_autoextend_increment  = @global_start_value;
