SET @start_global_value = @@global.block_encryption_mode;
SELECT @start_global_value;
@start_global_value
aes-128-ecb
SET @start_session_value = @@session.block_encryption_mode;
SELECT @start_session_value;
@start_session_value
aes-128-ecb
'#--------------------FN_DYNVARS_002_01-------------------------#'
SET @@global.block_encryption_mode = 'aes-192-ecb';
SET @@global.block_encryption_mode = DEFAULT;
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-128-ecb
SET @@session.block_encryption_mode = 'aes-192-ecb';
SET @@session.block_encryption_mode = DEFAULT;
SELECT @@session.block_encryption_mode;
@@session.block_encryption_mode
aes-128-ecb
'#--------------------FN_DYNVARS_002_02-------------------------#'
SET @@global.block_encryption_mode = @start_global_value;
SELECT @@global.block_encryption_mode = 'aes-128-ecb';
@@global.block_encryption_mode = 'aes-128-ecb'
1
SET @@session.block_encryption_mode = @start_session_value;
SELECT @@session.block_encryption_mode = 'aes-128-ecb';
@@session.block_encryption_mode = 'aes-128-ecb'
1
'#--------------------FN_DYNVARS_002_03-------------------------#'
SET @@global.block_encryption_mode = 'aes-128-ecb';
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-128-ecb
SET @@global.block_encryption_mode = 'aes-256-ecb';
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-256-ecb
SET @@global.block_encryption_mode = 'aes-256-cbc';
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-256-cbc
'#--------------------FN_DYNVARS_002_04-------------------------#'
SET @@session.block_encryption_mode = 'aes-128-ecb';
SELECT @@session.block_encryption_mode;
@@session.block_encryption_mode
aes-128-ecb
SET @@session.block_encryption_mode = 'aes-256-ecb';
SELECT @@session.block_encryption_mode;
@@session.block_encryption_mode
aes-256-ecb
SET @@session.block_encryption_mode = 'aes-256-cbc';
SELECT @@session.block_encryption_mode;
@@session.block_encryption_mode
aes-256-cbc
'#------------------FN_DYNVARS_002_05-----------------------#'
SET @@global.block_encryption_mode = 0;
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-128-ecb
SET @@global.block_encryption_mode = -1024;
ERROR 42000: Variable 'block_encryption_mode' can't be set to the value of '-1024'
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-128-ecb
SET @@global.block_encryption_mode = 65536;
ERROR 42000: Variable 'block_encryption_mode' can't be set to the value of '65536'
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-128-ecb
SET @@global.block_encryption_mode = 4294967295;
ERROR 42000: Variable 'block_encryption_mode' can't be set to the value of '4294967295'
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-128-ecb
SET @@global.block_encryption_mode = 65535.4;
ERROR 42000: Incorrect argument type to variable 'block_encryption_mode'
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-128-ecb
SET @@global.block_encryption_mode = ON;
ERROR 42000: Variable 'block_encryption_mode' can't be set to the value of 'ON'
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-128-ecb
SET @@global.block_encryption_mode = OFF;
ERROR 42000: Variable 'block_encryption_mode' can't be set to the value of 'OFF'
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-128-ecb
SET @@global.block_encryption_mode = test;
ERROR 42000: Variable 'block_encryption_mode' can't be set to the value of 'test'
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-128-ecb
SET @@global.block_encryption_mode = 'des-128-ecb';
ERROR 42000: Variable 'block_encryption_mode' can't be set to the value of 'des-128-ecb'
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-128-ecb
SET @@session.block_encryption_mode = 0;
SELECT @@session.block_encryption_mode;
@@session.block_encryption_mode
aes-128-ecb
SET @@session.block_encryption_mode = -1024;
ERROR 42000: Variable 'block_encryption_mode' can't be set to the value of '-1024'
SELECT @@session.block_encryption_mode;
@@session.block_encryption_mode
aes-128-ecb
SET @@session.block_encryption_mode = 65536;
ERROR 42000: Variable 'block_encryption_mode' can't be set to the value of '65536'
SELECT @@session.block_encryption_mode;
@@session.block_encryption_mode
aes-128-ecb
SET @@session.block_encryption_mode = 4294967295;
ERROR 42000: Variable 'block_encryption_mode' can't be set to the value of '4294967295'
SELECT @@session.block_encryption_mode;
@@session.block_encryption_mode
aes-128-ecb
SET @@session.block_encryption_mode = 65535.4;
ERROR 42000: Incorrect argument type to variable 'block_encryption_mode'
SELECT @@session.block_encryption_mode;
@@session.block_encryption_mode
aes-128-ecb
SET @@session.block_encryption_mode = ON;
ERROR 42000: Variable 'block_encryption_mode' can't be set to the value of 'ON'
SELECT @@session.block_encryption_mode;
@@session.block_encryption_mode
aes-128-ecb
SET @@session.block_encryption_mode = OFF;
ERROR 42000: Variable 'block_encryption_mode' can't be set to the value of 'OFF'
SELECT @@session.block_encryption_mode;
@@session.block_encryption_mode
aes-128-ecb
SET @@session.block_encryption_mode = test;
ERROR 42000: Variable 'block_encryption_mode' can't be set to the value of 'test'
SELECT @@session.block_encryption_mode;
@@session.block_encryption_mode
aes-128-ecb
SET @@session.block_encryption_mode = 'des-128-ecb';
ERROR 42000: Variable 'block_encryption_mode' can't be set to the value of 'des-128-ecb'
SELECT @@session.block_encryption_mode;
@@session.block_encryption_mode
aes-128-ecb
'#------------------FN_DYNVARS_002_06-----------------------#'
SELECT @@global.block_encryption_mode = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='block_encryption_mode';
@@global.block_encryption_mode = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_002_07-----------------------#'
SELECT @@session.block_encryption_mode = VARIABLE_VALUE
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='block_encryption_mode';
@@session.block_encryption_mode = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_002_08-----------------------#'
SET @@global.block_encryption_mode = TRUE;
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-192-ecb
SET @@global.block_encryption_mode = FALSE;
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-128-ecb
'#---------------------FN_DYNVARS_001_09----------------------#'
SET @@global.block_encryption_mode = 'aes-192-ecb';
SET @@session.block_encryption_mode = 'aes-256-cbc';
SELECT @@block_encryption_mode = @@global.block_encryption_mode;
@@block_encryption_mode = @@global.block_encryption_mode
0
'#---------------------FN_DYNVARS_001_10----------------------#'
SET @@block_encryption_mode = 'aes-192-cbc';
SELECT @@block_encryption_mode = @@local.block_encryption_mode;
@@block_encryption_mode = @@local.block_encryption_mode
1
SELECT @@local.block_encryption_mode = @@session.block_encryption_mode;
@@local.block_encryption_mode = @@session.block_encryption_mode
1
'#---------------------FN_DYNVARS_001_11----------------------#'
SET block_encryption_mode = 'aes-128-cbc';
SELECT @@block_encryption_mode;
@@block_encryption_mode
aes-128-cbc
SELECT local.block_encryption_mode;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.block_encryption_mode;
ERROR 42S02: Unknown table 'session' in field list
SELECT block_encryption_mode = @@session.block_encryption_mode;
ERROR 42S22: Unknown column 'block_encryption_mode' in 'field list'
SET @@global.block_encryption_mode = @start_global_value;
SELECT @@global.block_encryption_mode;
@@global.block_encryption_mode
aes-128-ecb
SET @@session.block_encryption_mode = @start_session_value;
SELECT @@session.block_encryption_mode;
@@session.block_encryption_mode
aes-128-ecb
#
# End of 5.7 tests
#
