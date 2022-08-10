SET @start_value = @@global.key_cache_division_limit;
SELECT @start_value;
@start_value
100
'#--------------------FN_DYNVARS_058_01------------------------#'
SET @@global.key_cache_division_limit = DEFAULT;
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
100
'#---------------------FN_DYNVARS_058_02-------------------------#'
SET @@global.key_cache_division_limit = @start_value;
SELECT @@global.key_cache_division_limit = 100;
@@global.key_cache_division_limit = 100
1
'#--------------------FN_DYNVARS_058_03------------------------#'
SET @@global.key_cache_division_limit = 1;
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
1
SET @@global.key_cache_division_limit = 50;
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
50
SET @@global.key_cache_division_limit = 99;
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
99
SET @@global.key_cache_division_limit = 2;
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
2
'#--------------------FN_DYNVARS_058_04-------------------------#'
SET @@global.key_cache_division_limit = -1;
Warnings:
Warning	1292	Truncated incorrect key_cache_division_limit value: '-1'
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
1
SET @@global.key_cache_division_limit = 101;
Warnings:
Warning	1292	Truncated incorrect key_cache_division_limit value: '101'
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
100
SET @@global.key_cache_division_limit = 10000.01;
ERROR 42000: Incorrect argument type to variable 'key_cache_division_limit'
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
100
SET @@global.key_cache_division_limit = -1024;
Warnings:
Warning	1292	Truncated incorrect key_cache_division_limit value: '-1024'
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
1
SET @@global.key_cache_division_limit = 0;
Warnings:
Warning	1292	Truncated incorrect key_cache_division_limit value: '0'
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
1
SET @@global.key_cache_division_limit = 200;
Warnings:
Warning	1292	Truncated incorrect key_cache_division_limit value: '200'
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
100
SET @@global.key_cache_division_limit = 65535;
Warnings:
Warning	1292	Truncated incorrect key_cache_division_limit value: '65535'
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
100
SET @@global.key_cache_division_limit = ' ';
ERROR 42000: Incorrect argument type to variable 'key_cache_division_limit'
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
100
SET @@global.key_cache_division_limit = ON;
ERROR 42000: Incorrect argument type to variable 'key_cache_division_limit'
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
100
SET @@global.key_cache_division_limit = 'test';
ERROR 42000: Incorrect argument type to variable 'key_cache_division_limit'
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
100
'#-------------------FN_DYNVARS_058_05----------------------------#'
SET @@session.key_cache_division_limit = 0;
ERROR HY000: Variable 'key_cache_division_limit' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@session.key_cache_division_limit;
ERROR HY000: Variable 'key_cache_division_limit' is a GLOBAL variable
'#----------------------FN_DYNVARS_058_06------------------------#'
SELECT @@global.key_cache_division_limit = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='key_cache_division_limit';
@@global.key_cache_division_limit = VARIABLE_VALUE
1
SELECT @@key_cache_division_limit = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='key_cache_division_limit';
@@key_cache_division_limit = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_058_07----------------------#'
SET @@global.key_cache_division_limit = TRUE;
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
1
SET @@global.key_cache_division_limit = FALSE;
Warnings:
Warning	1292	Truncated incorrect key_cache_division_limit value: '0'
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
1
'#---------------------FN_DYNVARS_058_08----------------------#'
SET @@global.key_cache_division_limit = 90;
SELECT @@key_cache_division_limit = @@global.key_cache_division_limit;
@@key_cache_division_limit = @@global.key_cache_division_limit
1
'#---------------------FN_DYNVARS_058_09----------------------#'
SET key_cache_division_limit = 80;
ERROR HY000: Variable 'key_cache_division_limit' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@key_cache_division_limit;
@@key_cache_division_limit
90
SET local.key_cache_division_limit = 10;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.key_cache_division_limit = 10' at line 1
SELECT local.key_cache_division_limit;
ERROR 42S02: Unknown table 'local' in field list
SET global.key_cache_division_limit = 10;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.key_cache_division_limit = 10' at line 1
SELECT global.key_cache_division_limit;
ERROR 42S02: Unknown table 'global' in field list
SELECT key_cache_division_limit;
ERROR 42S22: Unknown column 'key_cache_division_limit' in 'field list'
SELECT @@session.key_cache_division_limit;
ERROR HY000: Variable 'key_cache_division_limit' is a GLOBAL variable
SET @@global.key_cache_division_limit = @start_value;
SELECT @@global.key_cache_division_limit;
@@global.key_cache_division_limit
100
