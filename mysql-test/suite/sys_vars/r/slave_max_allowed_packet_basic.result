SET @start_value = @@global.slave_max_allowed_packet;
SELECT @start_value;
@start_value
1073741824
'#--------------------FN_DYNVARS_072_01------------------------#'
SET @@global.slave_max_allowed_packet = 5000;
Warnings:
Warning	1292	Truncated incorrect slave_max_allowed_packet value: '5000'
SET @@global.slave_max_allowed_packet = DEFAULT;
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1073741824
'#---------------------FN_DYNVARS_072_02-------------------------#'
SET @@global.slave_max_allowed_packet = @start_value;
SELECT @@global.slave_max_allowed_packet = 1073741824;
@@global.slave_max_allowed_packet = 1073741824
1
'Bug# 34876: Incorrect Default Value is assigned to variable';
'#--------------------FN_DYNVARS_072_03------------------------#'
SET @@global.slave_max_allowed_packet = 1024;
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1024
SET @@global.slave_max_allowed_packet = 1073741824;
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1073741824
SET @@global.slave_max_allowed_packet = 1073741824;
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1073741824
SET @@global.slave_max_allowed_packet = 1025;
Warnings:
Warning	1292	Truncated incorrect slave_max_allowed_packet value: '1025'
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1024
SET @@global.slave_max_allowed_packet = 65535;
Warnings:
Warning	1292	Truncated incorrect slave_max_allowed_packet value: '65535'
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
64512
'Bug# 34877: Invalid Values are coming in variable on assigning valid values';
'#--------------------FN_DYNVARS_072_04-------------------------#'
SET @@global.slave_max_allowed_packet = -1;
Warnings:
Warning	1292	Truncated incorrect slave_max_allowed_packet value: '-1'
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1024
SET @@global.slave_max_allowed_packet = 100000000000;
Warnings:
Warning	1292	Truncated incorrect slave_max_allowed_packet value: '100000000000'
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1073741824
SET @@global.slave_max_allowed_packet = 10000.01;
ERROR 42000: Incorrect argument type to variable 'slave_max_allowed_packet'
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1073741824
SET @@global.slave_max_allowed_packet = -1024;
Warnings:
Warning	1292	Truncated incorrect slave_max_allowed_packet value: '-1024'
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1024
SET @@global.slave_max_allowed_packet = 4294967296;
Warnings:
Warning	1292	Truncated incorrect slave_max_allowed_packet value: '4294967296'
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1073741824
SET @@global.slave_max_allowed_packet = 1023;
Warnings:
Warning	1292	Truncated incorrect slave_max_allowed_packet value: '1023'
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1024
'Bug # 34837: Errors are not coming on assigning invalid values to variable';
SET @@global.slave_max_allowed_packet = ON;
ERROR 42000: Incorrect argument type to variable 'slave_max_allowed_packet'
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1024
SET @@global.slave_max_allowed_packet = 'test';
ERROR 42000: Incorrect argument type to variable 'slave_max_allowed_packet'
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1024
'#-------------------FN_DYNVARS_072_05----------------------------#'
SET @@session.slave_max_allowed_packet = 4096;
ERROR HY000: Variable 'slave_max_allowed_packet' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@session.slave_max_allowed_packet;
ERROR HY000: Variable 'slave_max_allowed_packet' is a GLOBAL variable
'#----------------------FN_DYNVARS_072_06------------------------#'
SELECT @@global.slave_max_allowed_packet = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='slave_max_allowed_packet';
@@global.slave_max_allowed_packet = VARIABLE_VALUE
1
SELECT @@slave_max_allowed_packet = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='slave_max_allowed_packet';
@@slave_max_allowed_packet = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_072_07----------------------#'
SET @@global.slave_max_allowed_packet = TRUE;
Warnings:
Warning	1292	Truncated incorrect slave_max_allowed_packet value: '1'
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1024
SET @@global.slave_max_allowed_packet = FALSE;
Warnings:
Warning	1292	Truncated incorrect slave_max_allowed_packet value: '0'
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1024
'#---------------------FN_DYNVARS_072_08----------------------#'
SET @@global.slave_max_allowed_packet = 5000;
Warnings:
Warning	1292	Truncated incorrect slave_max_allowed_packet value: '5000'
SELECT @@slave_max_allowed_packet = @@global.slave_max_allowed_packet;
@@slave_max_allowed_packet = @@global.slave_max_allowed_packet
1
'#---------------------FN_DYNVARS_072_09----------------------#'
SET slave_max_allowed_packet = 6000;
ERROR HY000: Variable 'slave_max_allowed_packet' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@slave_max_allowed_packet;
@@slave_max_allowed_packet
4096
SET local.slave_max_allowed_packet = 7000;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.slave_max_allowed_packet = 7000' at line 1
SELECT local.slave_max_allowed_packet;
ERROR 42S02: Unknown table 'local' in field list
SET global.slave_max_allowed_packet = 8000;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.slave_max_allowed_packet = 8000' at line 1
SELECT global.slave_max_allowed_packet;
ERROR 42S02: Unknown table 'global' in field list
SELECT slave_max_allowed_packet = @@session.slave_max_allowed_packet;
ERROR 42S22: Unknown column 'slave_max_allowed_packet' in 'field list'
SET @@global.slave_max_allowed_packet = @start_value;
SELECT @@global.slave_max_allowed_packet;
@@global.slave_max_allowed_packet
1073741824
