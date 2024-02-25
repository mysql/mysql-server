SET @start_value = @@global.require_secure_transport;
SELECT @start_value;
@start_value
0
'#--------------------FN_DYNVARS_139_01------------------------#'
SET @@global.require_secure_transport = 1;
SET @@global.require_secure_transport = DEFAULT;
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
0
'#---------------------FN_DYNVARS_139_02-------------------------#'
SET @@global.require_secure_transport = @start_value;
SELECT @@global.require_secure_transport = 0;
@@global.require_secure_transport = 0
1
'#--------------------FN_DYNVARS_139_03------------------------#'
SET @@global.require_secure_transport = 0;
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
0
SET @@global.require_secure_transport = 1;
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
1
SET @@global.require_secure_transport = TRUE;
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
1
SET @@global.require_secure_transport = FALSE;
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
0
SET @@global.require_secure_transport = ON;
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
1
SET @@global.require_secure_transport = OFF;
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
0
'#--------------------FN_DYNVARS_139_04-------------------------#'
SET @@global.require_secure_transport = -1;
ERROR 42000: Variable 'require_secure_transport' can't be set to the value of '-1'
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
0
SET @@global.require_secure_transport = 4294967296;
ERROR 42000: Variable 'require_secure_transport' can't be set to the value of '4294967296'
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
0
SET @@global.require_secure_transport = 10240022115;
ERROR 42000: Variable 'require_secure_transport' can't be set to the value of '10240022115'
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
0
SET @@global.require_secure_transport = 10000.01;
ERROR 42000: Incorrect argument type to variable 'require_secure_transport'
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
0
SET @@global.require_secure_transport = -1024;
ERROR 42000: Variable 'require_secure_transport' can't be set to the value of '-1024'
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
0
SET @@global.require_secure_transport = 42949672950;
ERROR 42000: Variable 'require_secure_transport' can't be set to the value of '42949672950'
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
0
SET @@global.require_secure_transport = 'test';
ERROR 42000: Variable 'require_secure_transport' can't be set to the value of 'test'
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
0
'#-------------------FN_DYNVARS_139_05----------------------------#'
SET @@session.require_secure_transport = 0;
ERROR HY000: Variable 'require_secure_transport' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@require_secure_transport;
@@require_secure_transport
0
'#----------------------FN_DYNVARS_139_06------------------------#'
SELECT IF(@@global.require_secure_transport, "ON", "OFF") = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='require_secure_transport';
IF(@@global.require_secure_transport, "ON", "OFF") = VARIABLE_VALUE
1
SELECT IF(@@require_secure_transport, "ON", "OFF") = VARIABLE_VALUE
FROM performance_schema.session_variables
WHERE VARIABLE_NAME='require_secure_transport';
IF(@@require_secure_transport, "ON", "OFF") = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_139_07----------------------#'
SET @@global.require_secure_transport = 1;
SELECT @@require_secure_transport = @@global.require_secure_transport;
@@require_secure_transport = @@global.require_secure_transport
1
'#---------------------FN_DYNVARS_139_08----------------------#'
SET require_secure_transport = 1;
ERROR HY000: Variable 'require_secure_transport' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@require_secure_transport;
@@require_secure_transport
1
SET local.require_secure_transport = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.require_secure_transport = 1' at line 1
SELECT local.require_secure_transport;
ERROR 42S02: Unknown table 'local' in field list
SET global.require_secure_transport = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.require_secure_transport = 1' at line 1
SELECT global.require_secure_transport;
ERROR 42S02: Unknown table 'global' in field list
SELECT require_secure_transport;
ERROR 42S22: Unknown column 'require_secure_transport' in 'field list'
SELECT @@session.require_secure_transport;
ERROR HY000: Variable 'require_secure_transport' is a GLOBAL variable
SET @@global.require_secure_transport = @start_value;
SELECT @@global.require_secure_transport;
@@global.require_secure_transport
0
