SET @start_global_value = @@global.myisam_data_pointer_size;
SELECT @start_global_value;
@start_global_value
6
'#--------------------FN_DYNVARS_093_01-------------------------#'
SET @@global.myisam_data_pointer_size = 5;
SET @@global.myisam_data_pointer_size = DEFAULT;
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
6
'#--------------------FN_DYNVARS_093_02-------------------------#'
SET @@global.myisam_data_pointer_size = DEFAULT;
SELECT @@global.myisam_data_pointer_size = 6;
@@global.myisam_data_pointer_size = 6
1
'#--------------------FN_DYNVARS_093_03-------------------------#'
SET @@global.myisam_data_pointer_size = 2;
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
2
SET @@global.myisam_data_pointer_size = 3;
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
3
SET @@global.myisam_data_pointer_size = 4;
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
4
SET @@global.myisam_data_pointer_size = 5;
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
5
SET @@global.myisam_data_pointer_size = 6;
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
6
SET @@global.myisam_data_pointer_size = 7;
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
7
'#--------------------FN_DYNVARS_093_04-------------------------#'
SET @@myisam_data_pointer_size = 2;
ERROR HY000: Variable 'myisam_data_pointer_size' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.myisam_data_pointer_size = 3;
ERROR HY000: Variable 'myisam_data_pointer_size' is a GLOBAL variable and should be set with SET GLOBAL
SET @@local.myisam_data_pointer_size = 4;
ERROR HY000: Variable 'myisam_data_pointer_size' is a GLOBAL variable and should be set with SET GLOBAL
'#------------------FN_DYNVARS_093_05-----------------------#'
SET @@global.myisam_data_pointer_size = -1;
Warnings:
Warning	1292	Truncated incorrect myisam_data_pointer_size value: '-1'
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
2
SET @@global.myisam_data_pointer_size = 8;
Warnings:
Warning	1292	Truncated incorrect myisam_data_pointer_size value: '8'
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
7
SET @@global.myisam_data_pointer_size = 1;
Warnings:
Warning	1292	Truncated incorrect myisam_data_pointer_size value: '1'
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
2
SET @@global.myisam_data_pointer_size = 4294967296;
Warnings:
Warning	1292	Truncated incorrect myisam_data_pointer_size value: '4294967296'
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
7
SET @@global.myisam_data_pointer_size = -1024;
Warnings:
Warning	1292	Truncated incorrect myisam_data_pointer_size value: '-1024'
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
2
SET @@global.myisam_data_pointer_size = 65530.34;
ERROR 42000: Incorrect argument type to variable 'myisam_data_pointer_size'
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
2
SET @@global.myisam_data_pointer_size = two;
ERROR 42000: Incorrect argument type to variable 'myisam_data_pointer_size'
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
2
SET @@global.myisam_data_pointer_size = ' ';
ERROR 42000: Incorrect argument type to variable 'myisam_data_pointer_size'
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
2
'#------------------FN_DYNVARS_093_06-----------------------#'
SET @@global.myisam_data_pointer_size = 3;
SELECT @@global.myisam_data_pointer_size = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='myisam_data_pointer_size';
@@global.myisam_data_pointer_size = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_093_07-----------------------#'
SELECT count(VARIABLE_VALUE) 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='myisam_data_pointer_size';
count(VARIABLE_VALUE)
1
'#------------------FN_DYNVARS_093_08-----------------------#'
SET @@global.myisam_data_pointer_size = TRUE;
Warnings:
Warning	1292	Truncated incorrect myisam_data_pointer_size value: '1'
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
2
SET @@global.myisam_data_pointer_size = FALSE;
Warnings:
Warning	1292	Truncated incorrect myisam_data_pointer_size value: '0'
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
2
'#---------------------FN_DYNVARS_001_09----------------------#'
SET @@global.myisam_data_pointer_size = 5;
SELECT @@myisam_data_pointer_size = @@global.myisam_data_pointer_size;
@@myisam_data_pointer_size = @@global.myisam_data_pointer_size
1
'#---------------------FN_DYNVARS_001_10----------------------#'
SET myisam_data_pointer_size = 7;
ERROR HY000: Variable 'myisam_data_pointer_size' is a GLOBAL variable and should be set with SET GLOBAL
SELECT myisam_data_pointer_size;
ERROR 42S22: Unknown column 'myisam_data_pointer_size' in 'field list'
SELECT @@myisam_data_pointer_size;
@@myisam_data_pointer_size
5
SET global myisam_data_pointer_size = 2;
SET @@global.myisam_data_pointer_size = @start_global_value;
SELECT @@global.myisam_data_pointer_size;
@@global.myisam_data_pointer_size
6
