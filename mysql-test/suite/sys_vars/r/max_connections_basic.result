SET @start_value = @@global.max_connections;
SELECT @start_value;
@start_value
151
'#--------------------FN_DYNVARS_074_01------------------------#'
SET @@global.max_connections = 5000;
SET @@global.max_connections = DEFAULT;
SELECT @@global.max_connections;
@@global.max_connections
151
'#---------------------FN_DYNVARS_074_02-------------------------#'
SET @@global.max_connections = @start_value;
SELECT @@global.max_connections = 151;
@@global.max_connections = 151
1
'#--------------------FN_DYNVARS_074_03------------------------#'
SET @@global.max_connections = 100000;
SELECT @@global.max_connections;
@@global.max_connections
100000
SET @@global.max_connections = 99999;
SELECT @@global.max_connections;
@@global.max_connections
99999
SET @@global.max_connections = 65536;
SELECT @@global.max_connections;
@@global.max_connections
65536
SET @@global.max_connections = 1;
SELECT @@global.max_connections;
@@global.max_connections
1
SET @@global.max_connections = 2;
SELECT @@global.max_connections;
@@global.max_connections
2
'#--------------------FN_DYNVARS_074_04-------------------------#'
SET @@global.max_connections = -1;
Warnings:
Warning	1292	Truncated incorrect max_connections value: '-1'
SELECT @@global.max_connections;
@@global.max_connections
1
SET @@global.max_connections = 100000000000;
Warnings:
Warning	1292	Truncated incorrect max_connections value: '100000000000'
SELECT @@global.max_connections;
@@global.max_connections
100000
SET @@global.max_connections = 10000.01;
ERROR 42000: Incorrect argument type to variable 'max_connections'
SELECT @@global.max_connections;
@@global.max_connections
100000
SET @@global.max_connections = -1024;
Warnings:
Warning	1292	Truncated incorrect max_connections value: '-1024'
SELECT @@global.max_connections;
@@global.max_connections
1
SET @@global.max_connections = 0;
Warnings:
Warning	1292	Truncated incorrect max_connections value: '0'
SELECT @@global.max_connections;
@@global.max_connections
1
SET @@global.max_connections = 100001;
Warnings:
Warning	1292	Truncated incorrect max_connections value: '100001'
SELECT @@global.max_connections;
@@global.max_connections
100000
SET @@global.max_connections = ON;
ERROR 42000: Incorrect argument type to variable 'max_connections'
SELECT @@global.max_connections;
@@global.max_connections
100000
SET @@global.max_connections = 'test';
ERROR 42000: Incorrect argument type to variable 'max_connections'
SELECT @@global.max_connections;
@@global.max_connections
100000
SET @@global.max_connections = ' ';
ERROR 42000: Incorrect argument type to variable 'max_connections'
SELECT @@global.max_connections;
@@global.max_connections
100000
'#-------------------FN_DYNVARS_074_05----------------------------#'
SET @@session.max_connections = 4096;
ERROR HY000: Variable 'max_connections' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@session.max_connections;
ERROR HY000: Variable 'max_connections' is a GLOBAL variable
'#----------------------FN_DYNVARS_074_06------------------------#'
SELECT @@global.max_connections = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='max_connections';
@@global.max_connections = VARIABLE_VALUE
1
SELECT @@max_connections = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='max_connections';
@@max_connections = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_074_07----------------------#'
SET @@global.max_connections = TRUE;
SELECT @@global.max_connections;
@@global.max_connections
1
SET @@global.max_connections = FALSE;
Warnings:
Warning	1292	Truncated incorrect max_connections value: '0'
SELECT @@global.max_connections;
@@global.max_connections
1
'#---------------------FN_DYNVARS_074_08----------------------#'
SET @@global.max_connections = 5000;
SELECT @@max_connections = @@global.max_connections;
@@max_connections = @@global.max_connections
1
'#---------------------FN_DYNVARS_074_09----------------------#'
SET max_connections = 6000;
ERROR HY000: Variable 'max_connections' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@max_connections;
@@max_connections
5000
SET local.max_connections = 7000;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.max_connections = 7000' at line 1
SELECT local.max_connections;
ERROR 42S02: Unknown table 'local' in field list
SET global.max_connections = 8000;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.max_connections = 8000' at line 1
SELECT global.max_connections;
ERROR 42S02: Unknown table 'global' in field list
SELECT max_connections;
ERROR 42S22: Unknown column 'max_connections' in 'field list'
SELECT @@session.max_connections;
ERROR HY000: Variable 'max_connections' is a GLOBAL variable
SET @@global.max_connections = @start_value;
SELECT @@global.max_connections;
@@global.max_connections
151
