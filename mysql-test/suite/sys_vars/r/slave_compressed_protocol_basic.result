SET @global_start_value = @@global.slave_compressed_protocol;
SELECT @global_start_value;
@global_start_value
0
'#--------------------FN_DYNVARS_147_01------------------------#'
SET @@global.slave_compressed_protocol = 1;
SET @@global.slave_compressed_protocol = DEFAULT;
SELECT @@global.slave_compressed_protocol;
@@global.slave_compressed_protocol
0
'#---------------------FN_DYNVARS_147_02-------------------------#'
SET slave_compressed_protocol = 1;
ERROR HY000: Variable 'slave_compressed_protocol' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@slave_compressed_protocol;
@@slave_compressed_protocol
0
SELECT global.slave_compressed_protocol;
ERROR 42S02: Unknown table 'global' in field list
SET global slave_compressed_protocol = 1;
SELECT @@global.slave_compressed_protocol;
@@global.slave_compressed_protocol
1
'#--------------------FN_DYNVARS_147_03------------------------#'
SET @@global.slave_compressed_protocol = 0;
SELECT @@global.slave_compressed_protocol;
@@global.slave_compressed_protocol
0
SET @@global.slave_compressed_protocol = 1;
SELECT @@global.slave_compressed_protocol;
@@global.slave_compressed_protocol
1
'#--------------------FN_DYNVARS_147_04-------------------------#'
SET @@global.slave_compressed_protocol = -1;
ERROR 42000: Variable 'slave_compressed_protocol' can't be set to the value of '-1'
SET @@global.slave_compressed_protocol = 2;
ERROR 42000: Variable 'slave_compressed_protocol' can't be set to the value of '2'
SET @@global.slave_compressed_protocol = "1.00";
ERROR 42000: Variable 'slave_compressed_protocol' can't be set to the value of '1.00'
SET @@global.slave_compressed_protocol = "Y";
ERROR 42000: Variable 'slave_compressed_protocol' can't be set to the value of 'Y'
SET @@global.slave_compressed_protocol = YES;
ERROR 42000: Variable 'slave_compressed_protocol' can't be set to the value of 'YES'
SET @@global.slave_compressed_protocol = ONN;
ERROR 42000: Variable 'slave_compressed_protocol' can't be set to the value of 'ONN'
SET @@global.slave_compressed_protocol = 0FF;
ERROR 42000: Variable 'slave_compressed_protocol' can't be set to the value of '0FF'
SET @@global.slave_compressed_protocol = ' 1';
ERROR 42000: Variable 'slave_compressed_protocol' can't be set to the value of ' 1'
SET @@global.slave_compressed_protocol = NO;
ERROR 42000: Variable 'slave_compressed_protocol' can't be set to the value of 'NO'
'#-------------------FN_DYNVARS_147_05----------------------------#'
SET @@session.slave_compressed_protocol = 0;
ERROR HY000: Variable 'slave_compressed_protocol' is a GLOBAL variable and should be set with SET GLOBAL
SET @@slave_compressed_protocol = 0;
ERROR HY000: Variable 'slave_compressed_protocol' is a GLOBAL variable and should be set with SET GLOBAL
SET @@local.slave_compressed_protocol = 0;
ERROR HY000: Variable 'slave_compressed_protocol' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@session.slave_compressed_protocol;
ERROR HY000: Variable 'slave_compressed_protocol' is a GLOBAL variable
SELECT @@local.slave_compressed_protocol;
ERROR HY000: Variable 'slave_compressed_protocol' is a GLOBAL variable
'#----------------------FN_DYNVARS_147_06------------------------#'
SELECT count(VARIABLE_VALUE)
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='slave_compressed_protocol';
count(VARIABLE_VALUE)
1
'#----------------------FN_DYNVARS_147_07------------------------#'
SELECT IF(@@global.slave_compressed_protocol, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='slave_compressed_protocol';
IF(@@global.slave_compressed_protocol, "ON", "OFF") = VARIABLE_VALUE
1
SELECT @@global.slave_compressed_protocol;
@@global.slave_compressed_protocol
1
SELECT VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='slave_compressed_protocol';
VARIABLE_VALUE
ON
'#---------------------FN_DYNVARS_147_08-------------------------#'
SET @@global.slave_compressed_protocol = OFF;
SELECT @@global.slave_compressed_protocol;
@@global.slave_compressed_protocol
0
SET @@global.slave_compressed_protocol = ON;
SELECT @@global.slave_compressed_protocol;
@@global.slave_compressed_protocol
1
'#---------------------FN_DYNVARS_147_09----------------------#'
SET @@global.slave_compressed_protocol = TRUE;
SELECT @@global.slave_compressed_protocol;
@@global.slave_compressed_protocol
1
SET @@global.slave_compressed_protocol = FALSE;
SELECT @@global.slave_compressed_protocol;
@@global.slave_compressed_protocol
0
SET @@global.slave_compressed_protocol = @global_start_value;
SELECT @@global.slave_compressed_protocol;
@@global.slave_compressed_protocol
0
