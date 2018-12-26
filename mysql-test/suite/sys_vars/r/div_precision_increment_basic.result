SET @start_global_value = @@global.div_precision_increment;
SELECT @start_global_value;
@start_global_value
4
SET @start_session_value = @@session.div_precision_increment;
SELECT @start_session_value;
@start_session_value
4
'#--------------------FN_DYNVARS_027_01-------------------------#'
SET @@global.div_precision_increment = 100;
Warnings:
Warning	1292	Truncated incorrect div_precision_increment value: '100'
SET @@global.div_precision_increment = DEFAULT;
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
4
SET @@session.div_precision_increment = 200;
Warnings:
Warning	1292	Truncated incorrect div_precision_increment value: '200'
SET @@session.div_precision_increment = DEFAULT;
SELECT @@session.div_precision_increment;
@@session.div_precision_increment
4
'#--------------------FN_DYNVARS_027_02-------------------------#'
SET @@global.div_precision_increment = @start_global_value;
SELECT @@global.div_precision_increment = 4;
@@global.div_precision_increment = 4
1
SET @@session.div_precision_increment = @start_session_value;
SELECT @@session.div_precision_increment = 4;
@@session.div_precision_increment = 4
1
'#--------------------FN_DYNVARS_027_03-------------------------#'
SET @@global.div_precision_increment = 1;
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
1
SET @@global.div_precision_increment = 0;
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
0
SET @@global.div_precision_increment = 10;
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
10
SET @@global.div_precision_increment = 30;
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
30
'#--------------------FN_DYNVARS_027_04-------------------------#'
SET @@session.div_precision_increment = 1;
SELECT @@session.div_precision_increment;
@@session.div_precision_increment
1
SET @@session.div_precision_increment = 0;
SELECT @@session.div_precision_increment;
@@session.div_precision_increment
0
SET @@session.div_precision_increment = 7;
SELECT @@session.div_precision_increment;
@@session.div_precision_increment
7
SET @@session.div_precision_increment = 30;
SELECT @@session.div_precision_increment;
@@session.div_precision_increment
30
'#------------------FN_DYNVARS_027_05-----------------------#'
SET @@global.div_precision_increment = 31;
Warnings:
Warning	1292	Truncated incorrect div_precision_increment value: '31'
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
30
SET @@global.div_precision_increment = -1;
Warnings:
Warning	1292	Truncated incorrect div_precision_increment value: '-1'
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
0
SET @@global.div_precision_increment = -1024;
Warnings:
Warning	1292	Truncated incorrect div_precision_increment value: '-1024'
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
0
SET @@global.div_precision_increment = 65536;
Warnings:
Warning	1292	Truncated incorrect div_precision_increment value: '65536'
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
30
SET @@session.div_precision_increment = -1;
Warnings:
Warning	1292	Truncated incorrect div_precision_increment value: '-1'
SELECT @@session.div_precision_increment;
@@session.div_precision_increment
0
SET @@session.div_precision_increment = 31;
Warnings:
Warning	1292	Truncated incorrect div_precision_increment value: '31'
SELECT @@session.div_precision_increment;
@@session.div_precision_increment
30
SET @@session.div_precision_increment = -1042;
Warnings:
Warning	1292	Truncated incorrect div_precision_increment value: '-1042'
SELECT @@session.div_precision_increment;
@@session.div_precision_increment
0
SET @@session.div_precision_increment = 65550;
Warnings:
Warning	1292	Truncated incorrect div_precision_increment value: '65550'
SELECT @@session.div_precision_increment;
@@session.div_precision_increment
30
SET @@global.div_precision_increment = 65530.30;
ERROR 42000: Incorrect argument type to variable 'div_precision_increment'
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
30
SET @@global.div_precision_increment = OFF;
ERROR 42000: Incorrect argument type to variable 'div_precision_increment'
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
30
SET @@session.div_precision_increment = ON;
ERROR 42000: Incorrect argument type to variable 'div_precision_increment'
SELECT @@session.div_precision_increment;
@@session.div_precision_increment
30
SET @@session.div_precision_increment = 65530.30;
ERROR 42000: Incorrect argument type to variable 'div_precision_increment'
SELECT @@session.div_precision_increment;
@@session.div_precision_increment
30
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
30
SET @@global.div_precision_increment = " ";
ERROR 42000: Incorrect argument type to variable 'div_precision_increment'
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
30
SET @@session.div_precision_increment = " ";
ERROR 42000: Incorrect argument type to variable 'div_precision_increment'
SELECT @@session.div_precision_increment;
@@session.div_precision_increment
30
'#------------------FN_DYNVARS_027_06-----------------------#'
SELECT @@global.div_precision_increment = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='div_precision_increment';
@@global.div_precision_increment = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_027_07-----------------------#'
SELECT @@session.div_precision_increment = VARIABLE_VALUE FROM performance_schema.session_variables WHERE VARIABLE_NAME='div_precision_increment';
@@session.div_precision_increment = VARIABLE_VALUE
1
SET @@global.div_precision_increment = TRUE;
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
1
SET @@global.div_precision_increment = FALSE;
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
0
'#---------------------FN_DYNVARS_027_08----------------------#'
SET @@global.div_precision_increment = 0;
SELECT @@div_precision_increment = @@global.div_precision_increment;
@@div_precision_increment = @@global.div_precision_increment
0
'#---------------------FN_DYNVARS_027_09----------------------#'
SET @@div_precision_increment = 1;
SELECT @@div_precision_increment = @@local.div_precision_increment;
@@div_precision_increment = @@local.div_precision_increment
1
SELECT @@local.div_precision_increment = @@session.div_precision_increment;
@@local.div_precision_increment = @@session.div_precision_increment
1
'#---------------------FN_DYNVARS_027_10----------------------#'
SET div_precision_increment = 1;
SELECT @@div_precision_increment;
@@div_precision_increment
1
SET local.div_precision_increment = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.div_precision_increment = 1' at line 1
SELECT local.div_precision_increment;
ERROR 42S02: Unknown table 'local' in field list
SET session.div_precision_increment = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'session.div_precision_increment = 1' at line 1
SELECT session.div_precision_increment;
ERROR 42S02: Unknown table 'session' in field list
SELECT div_precision_increment = @@session.div_precision_increment;
ERROR 42S22: Unknown column 'div_precision_increment' in 'field list'
SET @@global.div_precision_increment = @start_global_value;
SELECT @@global.div_precision_increment;
@@global.div_precision_increment
4
SET @@session.div_precision_increment = @start_session_value;
SELECT @@session.div_precision_increment;
@@session.div_precision_increment
4
