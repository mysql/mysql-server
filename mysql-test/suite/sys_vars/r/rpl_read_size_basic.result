SET @start_rpl_read_size = @@global.rpl_read_size;
SELECT @start_rpl_read_size;
@start_rpl_read_size
8192
'#--------------------TEST 01------------------------#'
SET @@global.rpl_read_size = 102400;
SET @@global.rpl_read_size = DEFAULT;
SELECT @@global.rpl_read_size;
@@global.rpl_read_size
8192
'#---------------------TEST_02-------------------------#'
SET @@global.rpl_read_size = @start_rpl_read_size;
SELECT @@global.rpl_read_size = 8192;
@@global.rpl_read_size = 8192
1
'#--------------------TEST_03------------------------#'
SET @@global.rpl_read_size = 81920;
SELECT @@global.rpl_read_size;
@@global.rpl_read_size
81920
SET @@global.rpl_read_size = 102400;
SELECT @@global.rpl_read_size;
@@global.rpl_read_size
102400
'#--------------------TEST_04-------------------------#'
SET @@global.rpl_read_size = -1;
Warnings:
Warning	1292	Truncated incorrect rpl_read_size value: '-1'
SELECT @@global.rpl_read_size;
@@global.rpl_read_size
8192
SET @@global.rpl_read_size = 4096;
Warnings:
Warning	1292	Truncated incorrect rpl_read_size value: '4096'
SELECT @@global.rpl_read_size;
@@global.rpl_read_size
8192
SET @@global.rpl_read_size = 18446744073709551616;
ERROR 42000: Incorrect argument type to variable 'rpl_read_size'
SELECT @@global.rpl_read_size;
@@global.rpl_read_size
8192
SET @@global.rpl_read_size = 10000.01;
ERROR 42000: Incorrect argument type to variable 'rpl_read_size'
SELECT @@global.rpl_read_size;
@@global.rpl_read_size
8192
SET @@global.rpl_read_size = ON;
ERROR 42000: Incorrect argument type to variable 'rpl_read_size'
SELECT @@global.rpl_read_size;
@@global.rpl_read_size
8192
SET @@global.rpl_read_size = 'test';
ERROR 42000: Incorrect argument type to variable 'rpl_read_size'
SELECT @@global.rpl_read_size;
@@global.rpl_read_size
8192
'#-------------------TEST_05----------------------------#'
SET @@session.rpl_read_size = 8192;
ERROR HY000: Variable 'rpl_read_size' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@session.rpl_read_size;
ERROR HY000: Variable 'rpl_read_size' is a GLOBAL variable
'#----------------------TEST_06------------------------#'
SELECT @@global.rpl_read_size = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='rpl_read_size';
@@global.rpl_read_size = VARIABLE_VALUE
1
SELECT @@rpl_read_size = VARIABLE_VALUE
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='rpl_read_size';
@@rpl_read_size = VARIABLE_VALUE
1
'#---------------------TEST_07----------------------#'
SET @@global.rpl_read_size = 16384;
SELECT @@rpl_read_size = @@global.rpl_read_size;
@@rpl_read_size = @@global.rpl_read_size
1
'#---------------------TEST_08----------------------#'
SET rpl_read_size = 8192;
ERROR HY000: Variable 'rpl_read_size' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@rpl_read_size;
@@rpl_read_size
16384
SET local.rpl_read_size = 8192
--Error ER_UNKNOWN_TABLE
SELECT local.rpl_read_size;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.rpl_read_size = 8192
--Error ER_UNKNOWN_TABLE
SELECT local.rpl_read_size' at line 1
SET global.rpl_read_size = 8192;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.rpl_read_size = 8192' at line 1
SELECT global.rpl_read_size;
ERROR 42S02: Unknown table 'global' in field list
SELECT rpl_read_size;
ERROR 42S22: Unknown column 'rpl_read_size' in 'field list'
SELECT @@session.rpl_read_size;
ERROR HY000: Variable 'rpl_read_size' is a GLOBAL variable
SET @@global.rpl_read_size = @start_rpl_read_size;
SELECT @@global.rpl_read_size;
@@global.rpl_read_size
8192
