SET @start_global_value = @@global.tmp_table_size;
SET @start_session_value = @@session.tmp_table_size;
'#--------------------FN_DYNVARS_005_01-------------------------#'
SET @@global.tmp_table_size = 100;
Warnings:
Warning	1292	Truncated incorrect tmp_table_size value: '100'
SET @@global.tmp_table_size = DEFAULT;
SET @@session.tmp_table_size = 200;
Warnings:
Warning	1292	Truncated incorrect tmp_table_size value: '200'
SET @@session.tmp_table_size = DEFAULT;
'#--------------------FN_DYNVARS_005_02-------------------------#'
SELECT @@global.tmp_table_size >= 16777216;
@@global.tmp_table_size >= 16777216
1
SELECT @@session.tmp_table_size >= 16777216;
@@session.tmp_table_size >= 16777216
1
'#--------------------FN_DYNVARS_005_03-------------------------#'
SET @@global.tmp_table_size = 1024;
SELECT @@global.tmp_table_size;
@@global.tmp_table_size
1024
SET @@global.tmp_table_size = 60020;
SELECT @@global.tmp_table_size;
@@global.tmp_table_size
60020
SET @@global.tmp_table_size = 4294967295;
SELECT @@global.tmp_table_size;
@@global.tmp_table_size
4294967295
'#--------------------FN_DYNVARS_005_04-------------------------#'
SET @@session.tmp_table_size = 1024;
SELECT @@session.tmp_table_size;
@@session.tmp_table_size
1024
SET @@session.tmp_table_size = 4294967295;
SELECT @@session.tmp_table_size;
@@session.tmp_table_size
4294967295
SET @@session.tmp_table_size = 65535;
SELECT @@session.tmp_table_size;
@@session.tmp_table_size
65535
'#------------------FN_DYNVARS_005_05-----------------------#'
SET @@global.tmp_table_size = 0;
Warnings:
Warning	1292	Truncated incorrect tmp_table_size value: '0'
SELECT @@global.tmp_table_size;
@@global.tmp_table_size
1024
SET @@global.tmp_table_size = -1024;
Warnings:
Warning	1292	Truncated incorrect tmp_table_size value: '-1024'
SELECT @@global.tmp_table_size;
@@global.tmp_table_size
1024
SET @@global.tmp_table_size = 1000;
Warnings:
Warning	1292	Truncated incorrect tmp_table_size value: '1000'
SELECT @@global.tmp_table_size;
@@global.tmp_table_size
1024
SET @@global.tmp_table_size = 4294967299;
SELECT @@global.tmp_table_size;
@@global.tmp_table_size
4294967299
SET @@global.tmp_table_size = ' ';
ERROR 42000: Incorrect argument type to variable 'tmp_table_size'
SELECT @@global.tmp_table_size;
@@global.tmp_table_size
4294967299
SET @@global.tmp_table_size = ON;
ERROR 42000: Incorrect argument type to variable 'tmp_table_size'
SET @@global.tmp_table_size = OFF;
ERROR 42000: Incorrect argument type to variable 'tmp_table_size'
SET @@global.tmp_table_size = True;
Warnings:
Warning	1292	Truncated incorrect tmp_table_size value: '1'
SELECT @@global.tmp_table_size;
@@global.tmp_table_size
1024
SET @@global.tmp_table_size = False;
Warnings:
Warning	1292	Truncated incorrect tmp_table_size value: '0'
SELECT @@global.tmp_table_size;
@@global.tmp_table_size
1024
SET @@global.tmp_table_size = 65530.34;
ERROR 42000: Incorrect argument type to variable 'tmp_table_size'
SET @@global.tmp_table_size ="Test";
ERROR 42000: Incorrect argument type to variable 'tmp_table_size'
SET @@session.tmp_table_size = ON;
ERROR 42000: Incorrect argument type to variable 'tmp_table_size'
SET @@session.tmp_table_size = OFF;
ERROR 42000: Incorrect argument type to variable 'tmp_table_size'
SET @@session.tmp_table_size = True;
Warnings:
Warning	1292	Truncated incorrect tmp_table_size value: '1'
SELECT @@session.tmp_table_size;
@@session.tmp_table_size
1024
SET @@session.tmp_table_size = 65530.34;
ERROR 42000: Incorrect argument type to variable 'tmp_table_size'
SET @@session.tmp_table_size = False;
Warnings:
Warning	1292	Truncated incorrect tmp_table_size value: '0'
SELECT @@session.tmp_table_size;
@@session.tmp_table_size
1024
SET @@session.tmp_table_size = "Test";
ERROR 42000: Incorrect argument type to variable 'tmp_table_size'
SET @@session.tmp_table_size = ' ';
ERROR 42000: Incorrect argument type to variable 'tmp_table_size'
SELECT @@session.tmp_table_size;
@@session.tmp_table_size
1024
SET @@session.tmp_table_size = 1000;
Warnings:
Warning	1292	Truncated incorrect tmp_table_size value: '1000'
SELECT @@session.tmp_table_size;
@@session.tmp_table_size
1024
SET @@session.tmp_table_size = 12345678901;
SELECT @@session.tmp_table_size IN (12345678901,4294967295);
@@session.tmp_table_size IN (12345678901,4294967295)
1
'#------------------FN_DYNVARS_005_06-----------------------#'
SELECT @@global.tmp_table_size = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='tmp_table_size';
@@global.tmp_table_size = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_005_07-----------------------#'
SELECT @@session.tmp_table_size = VARIABLE_VALUE
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='tmp_table_size';
@@session.tmp_table_size = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_001_09----------------------#'
SET @@global.tmp_table_size = 1024;
SET @@tmp_table_size = 4294967295;
SELECT @@tmp_table_size = @@global.tmp_table_size;
@@tmp_table_size = @@global.tmp_table_size
0
'#---------------------FN_DYNVARS_001_10----------------------#'
SET @@tmp_table_size = 100;
Warnings:
Warning	1292	Truncated incorrect tmp_table_size value: '100'
SELECT @@tmp_table_size = @@local.tmp_table_size;
@@tmp_table_size = @@local.tmp_table_size
1
SELECT @@local.tmp_table_size = @@session.tmp_table_size;
@@local.tmp_table_size = @@session.tmp_table_size
1
'#---------------------FN_DYNVARS_001_11----------------------#'
SET tmp_table_size = 1027;
SELECT @@tmp_table_size;
@@tmp_table_size
1027
SELECT local.tmp_table_size;
ERROR 42S02: Unknown table 'local' in field list
SELECT global.tmp_table_size;
ERROR 42S02: Unknown table 'global' in field list
SELECT tmp_table_size = @@session.tmp_table_size;
ERROR 42S22: Unknown column 'tmp_table_size' in 'field list'
SET @@global.tmp_table_size = @start_global_value;
SET @@session.tmp_table_size = @start_session_value;
