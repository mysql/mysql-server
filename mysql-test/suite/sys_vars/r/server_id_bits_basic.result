SET @@global.general_log= 0;
SET @start_global_value = @@global.server_id_bits;
SELECT @start_global_value;
@start_global_value
32
'#--------------------FN_DYNVARS_144_01-------------------------#'
SET @@global.server_id_bits = 16;
SET @@global.server_id_bits = DEFAULT;
SELECT @@global.server_id_bits;
@@global.server_id_bits
32
'#--------------------FN_DYNVARS_144_02-------------------------#'
SET @@global.server_id_bits = DEFAULT;
SELECT @@global.server_id_bits = 0;
@@global.server_id_bits = 0
0
'#--------------------FN_DYNVARS_144_03-------------------------#'
SET @@global.server_id_bits = 0;
SELECT @@global.server_id_bits;
@@global.server_id_bits
0
SET @@global.server_id_bits = 1;
SELECT @@global.server_id_bits;
@@global.server_id_bits
1
SET @@global.server_id_bits = 15;
SELECT @@global.server_id_bits;
@@global.server_id_bits
15
SET @@global.server_id_bits = 31;
SELECT @@global.server_id_bits;
@@global.server_id_bits
31
SET @@global.server_id_bits = 32;
SELECT @@global.server_id_bits;
@@global.server_id_bits
32
'#--------------------FN_DYNVARS_144_04-------------------------#'
SET @@server_id_bits = 2;
ERROR HY000: Variable 'server_id_bits' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.server_id_bits = 3;
ERROR HY000: Variable 'server_id_bits' is a GLOBAL variable and should be set with SET GLOBAL
SET @@local.server_id_bits = 4;
ERROR HY000: Variable 'server_id_bits' is a GLOBAL variable and should be set with SET GLOBAL
'#------------------FN_DYNVARS_144_05-----------------------#'
SET @@global.server_id_bits = -1;
Warnings:
Warning	1292	Truncated incorrect server_id_bits value: '-1'
SELECT @@global.server_id_bits;
@@global.server_id_bits
0
SET @@global.server_id_bits = -2147483648;
Warnings:
Warning	1292	Truncated incorrect server_id_bits value: '-2147483648'
SELECT @@global.server_id_bits;
@@global.server_id_bits
0
SET @@global.server_id_bits = 2147483649*2;
Warnings:
Warning	1292	Truncated incorrect server_id_bits value: '4294967298'
SELECT @@global.server_id_bits;
@@global.server_id_bits
32
SET @@global.server_id_bits = 65530.34;
ERROR 42000: Incorrect argument type to variable 'server_id_bits'
SET @@global.server_id_bits = '125';
ERROR 42000: Incorrect argument type to variable 'server_id_bits'
SET @@global.server_id_bits = 7483649.56;
ERROR 42000: Incorrect argument type to variable 'server_id_bits'
SET @@global.server_id_bits = 1G;
ERROR 42000: Incorrect argument type to variable 'server_id_bits'
'#------------------FN_DYNVARS_144_06-----------------------#'
SET @@global.server_id_bits = 12;
SELECT @@global.server_id_bits = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='server_id_bits';
@@global.server_id_bits = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_144_07-----------------------#'
SELECT count(VARIABLE_VALUE) 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='server_id_bits';
count(VARIABLE_VALUE)
1
'#------------------FN_DYNVARS_144_08-----------------------#'
SET @@global.server_id_bits = TRUE;
SELECT @@global.server_id_bits;
@@global.server_id_bits
1
SET @@global.server_id_bits = FALSE;
SELECT @@global.server_id_bits;
@@global.server_id_bits
0
'#---------------------FN_DYNVARS_001_09----------------------#'
SET @@global.server_id_bits = 19;
SELECT @@server_id_bits = @@global.server_id_bits;
@@server_id_bits = @@global.server_id_bits
1
'#---------------------FN_DYNVARS_001_10----------------------#'
SET server_id_bits = 2048;
ERROR HY000: Variable 'server_id_bits' is a GLOBAL variable and should be set with SET GLOBAL
SELECT server_id_bits;
ERROR 42S22: Unknown column 'server_id_bits' in 'field list'
SELECT @@server_id_bits;
@@server_id_bits
19
SET global server_id_bits = 20;
SET @@global.server_id_bits = @start_global_value;
SELECT @@global.server_id_bits;
@@global.server_id_bits
32
SET @@global.general_log= 1;
