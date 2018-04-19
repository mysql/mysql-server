SET @start_value = @@global.key_cache_age_threshold;
SELECT @start_value;
@start_value
300
'#--------------------FN_DYNVARS_056_01------------------------#'
SET @@global.key_cache_age_threshold = DEFAULT;
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
300
'#---------------------FN_DYNVARS_056_02-------------------------#'
SET @@global.key_cache_age_threshold = @start_value;
SELECT @@global.key_cache_age_threshold = 300;
@@global.key_cache_age_threshold = 300
1
'#--------------------FN_DYNVARS_056_03------------------------#'
SET @@global.key_cache_age_threshold = 100;
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
100
SET @@global.key_cache_age_threshold = 4294967295;
Warnings:
Warning	1292	Truncated incorrect key_cache_age_threshold value: '4294967295'
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
4294967200
SET @@global.key_cache_age_threshold = 1800;
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
1800
SET @@global.key_cache_age_threshold = 65535;
Warnings:
Warning	1292	Truncated incorrect key_cache_age_threshold value: '65535'
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
65500
SET @@global.key_cache_age_threshold = 18446744073709551615;
Warnings:
Warning	1292	Truncated incorrect key_cache_age_threshold value: '18446744073709551615'
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
18446744073709551600
'#--------------------FN_DYNVARS_056_04-------------------------#'
SET @@global.key_cache_age_threshold = -1;
Warnings:
Warning	1292	Truncated incorrect key_cache_age_threshold value: '-1'
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
100
SET @@global.key_cache_age_threshold = 10000.01;
ERROR 42000: Incorrect argument type to variable 'key_cache_age_threshold'
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
100
SET @@global.key_cache_age_threshold = -1024;
Warnings:
Warning	1292	Truncated incorrect key_cache_age_threshold value: '-1024'
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
100
SET @@global.key_cache_age_threshold = 99;
Warnings:
Warning	1292	Truncated incorrect key_cache_age_threshold value: '99'
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
100
SET @@global.key_cache_age_threshold = ON;
ERROR 42000: Incorrect argument type to variable 'key_cache_age_threshold'
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
100
SET @@global.key_cache_age_threshold = 'test';
ERROR 42000: Incorrect argument type to variable 'key_cache_age_threshold'
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
100
SET @@global.key_cache_age_threshold = 18446744073709551616;
ERROR 42000: Incorrect argument type to variable 'key_cache_age_threshold'
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
100
'#-------------------FN_DYNVARS_056_05----------------------------#'
SET @@session.key_cache_age_threshold = 0;
ERROR HY000: Variable 'key_cache_age_threshold' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@session.key_cache_age_threshold;
ERROR HY000: Variable 'key_cache_age_threshold' is a GLOBAL variable
'#----------------------FN_DYNVARS_056_06------------------------#'
SELECT @@global.key_cache_age_threshold = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='key_cache_age_threshold';
@@global.key_cache_age_threshold = VARIABLE_VALUE
1
SELECT @@key_cache_age_threshold = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='key_cache_age_threshold';
@@key_cache_age_threshold = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_056_07----------------------#'
SET @@global.key_cache_age_threshold = TRUE;
Warnings:
Warning	1292	Truncated incorrect key_cache_age_threshold value: '1'
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
100
SET @@global.key_cache_age_threshold = FALSE;
Warnings:
Warning	1292	Truncated incorrect key_cache_age_threshold value: '0'
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
100
'#---------------------FN_DYNVARS_056_08----------------------#'
SET @@global.key_cache_age_threshold = 101;
Warnings:
Warning	1292	Truncated incorrect key_cache_age_threshold value: '101'
SELECT @@key_cache_age_threshold = @@global.key_cache_age_threshold;
@@key_cache_age_threshold = @@global.key_cache_age_threshold
1
'#---------------------FN_DYNVARS_056_09----------------------#'
SET key_cache_age_threshold = 8000;
ERROR HY000: Variable 'key_cache_age_threshold' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@key_cache_age_threshold;
@@key_cache_age_threshold
100
SET local.key_cache_age_threshold = 10;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.key_cache_age_threshold = 10' at line 1
SELECT local.key_cache_age_threshold;
ERROR 42S02: Unknown table 'local' in field list
SET global.key_cache_age_threshold = 10;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.key_cache_age_threshold = 10' at line 1
SELECT global.key_cache_age_threshold;
ERROR 42S02: Unknown table 'global' in field list
SELECT key_cache_age_threshold = @@session.key_cache_age_threshold;
ERROR 42S22: Unknown column 'key_cache_age_threshold' in 'field list'
SET @@global.key_cache_age_threshold = @start_value;
SELECT @@global.key_cache_age_threshold;
@@global.key_cache_age_threshold
300
