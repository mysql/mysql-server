SET @start_global_value = @@global.max_allowed_packet;
SELECT @start_global_value;
@start_global_value
67108864
SET @@global.max_allowed_packet = DEFAULT;
'#--------------------FN_DYNVARS_070_01-------------------------#'
SET @@global.max_allowed_packet = 1000;
Warnings:
Warning	1292	Truncated incorrect max_allowed_packet value: '1000'
Warning	1708	The value of 'max_allowed_packet' should be no less than the value of 'net_buffer_length'
SET @@global.max_allowed_packet = DEFAULT;
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
67108864
SET @@session.max_allowed_packet = 20000;
ERROR HY000: SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value
SET @@session.max_allowed_packet = DEFAULT;
ERROR HY000: SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value
SELECT @@session.max_allowed_packet;
@@session.max_allowed_packet
67108864
'#--------------------FN_DYNVARS_070_02-------------------------#'
SET @@global.max_allowed_packet = DEFAULT;
SELECT @@global.max_allowed_packet = 67108864;
@@global.max_allowed_packet = 67108864
1
'#--------------------FN_DYNVARS_070_03-------------------------#'
SET @@global.max_allowed_packet = 1024;
Warnings:
Warning	1708	The value of 'max_allowed_packet' should be no less than the value of 'net_buffer_length'
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
1024
SET @@global.max_allowed_packet = 1025;
Warnings:
Warning	1292	Truncated incorrect max_allowed_packet value: '1025'
Warning	1708	The value of 'max_allowed_packet' should be no less than the value of 'net_buffer_length'
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
1024
SET @@global.max_allowed_packet = 1073741824;
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
1073741824
SET @@global.max_allowed_packet = 1073741823;
Warnings:
Warning	1292	Truncated incorrect max_allowed_packet value: '1073741823'
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
1073740800
'#--------------------FN_DYNVARS_070_04-------------------------#'
SET @@session.max_allowed_packet = 1024;
ERROR HY000: SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value
SELECT @@session.max_allowed_packet;
@@session.max_allowed_packet
67108864
SET @@session.max_allowed_packet = 1025;
ERROR HY000: SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value
SELECT @@session.max_allowed_packet;
@@session.max_allowed_packet
67108864
SET @@session.max_allowed_packet = 65535;
ERROR HY000: SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value
SELECT @@session.max_allowed_packet;
@@session.max_allowed_packet
67108864
SET @@session.max_allowed_packet = 1073741824;
ERROR HY000: SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value
SELECT @@session.max_allowed_packet;
@@session.max_allowed_packet
67108864
SET @@session.max_allowed_packet = 1073741823;
ERROR HY000: SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value
SELECT @@session.max_allowed_packet;
@@session.max_allowed_packet
67108864
'#------------------FN_DYNVARS_070_05-----------------------#'
SET @@global.max_allowed_packet = 0;
Warnings:
Warning	1292	Truncated incorrect max_allowed_packet value: '0'
Warning	1708	The value of 'max_allowed_packet' should be no less than the value of 'net_buffer_length'
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
1024
SET @@global.max_allowed_packet = -1024;
Warnings:
Warning	1292	Truncated incorrect max_allowed_packet value: '-1024'
Warning	1708	The value of 'max_allowed_packet' should be no less than the value of 'net_buffer_length'
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
1024
SET @@global.max_allowed_packet = 1023;
Warnings:
Warning	1292	Truncated incorrect max_allowed_packet value: '1023'
Warning	1708	The value of 'max_allowed_packet' should be no less than the value of 'net_buffer_length'
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
1024
SET @@global.max_allowed_packet = 10737418241;
Warnings:
Warning	1292	Truncated incorrect max_allowed_packet value: '10737418241'
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
1073741824
SET @@global.max_allowed_packet = 1073741826;
Warnings:
Warning	1292	Truncated incorrect max_allowed_packet value: '1073741826'
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
1073741824
SET @@global.max_allowed_packet = 65530.34;
ERROR 42000: Incorrect argument type to variable 'max_allowed_packet'
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
1073741824
SET @@global.max_allowed_packet = test;
ERROR 42000: Incorrect argument type to variable 'max_allowed_packet'
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
1073741824
SET @@global.max_allowed_packet = ' ';
ERROR 42000: Incorrect argument type to variable 'max_allowed_packet'
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
1073741824
SET @@session.max_allowed_packet = 0;
ERROR HY000: SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value
SELECT @@session.max_allowed_packet;
@@session.max_allowed_packet
67108864
SET @@session.max_allowed_packet = 1023;
ERROR HY000: SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value
SELECT @@session.max_allowed_packet;
@@session.max_allowed_packet
67108864
SET @@session.max_allowed_packet = -2;
ERROR HY000: SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value
SELECT @@session.max_allowed_packet;
@@session.max_allowed_packet
67108864
SET @@session.max_allowed_packet = 65530.34;
ERROR 42000: Incorrect argument type to variable 'max_allowed_packet'
SET @@session.max_allowed_packet = 10737418241;
ERROR HY000: SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value
SELECT @@session.max_allowed_packet;
@@session.max_allowed_packet
67108864
SET @@session.max_allowed_packet = test;
ERROR 42000: Incorrect argument type to variable 'max_allowed_packet'
SELECT @@session.max_allowed_packet;
@@session.max_allowed_packet
67108864
SET @@session.max_allowed_packet = ' ';
ERROR 42000: Incorrect argument type to variable 'max_allowed_packet'
SELECT @@session.max_allowed_packet;
@@session.max_allowed_packet
67108864
'#------------------FN_DYNVARS_070_06-----------------------#'
SELECT @@global.max_allowed_packet = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='max_allowed_packet';
@@global.max_allowed_packet = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_070_07-----------------------#'
SELECT @@session.max_allowed_packet = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='max_allowed_packet';
@@session.max_allowed_packet = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_070_08-----------------------#'
SET @@global.max_allowed_packet = TRUE;
Warnings:
Warning	1292	Truncated incorrect max_allowed_packet value: '1'
Warning	1708	The value of 'max_allowed_packet' should be no less than the value of 'net_buffer_length'
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
1024
SET @@global.max_allowed_packet = FALSE;
Warnings:
Warning	1292	Truncated incorrect max_allowed_packet value: '0'
Warning	1708	The value of 'max_allowed_packet' should be no less than the value of 'net_buffer_length'
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
1024
'#---------------------FN_DYNVARS_070_09----------------------#'
SET @@global.max_allowed_packet = 2048;
Warnings:
Warning	1708	The value of 'max_allowed_packet' should be no less than the value of 'net_buffer_length'
SELECT @@max_allowed_packet = @@global.max_allowed_packet;
@@max_allowed_packet = @@global.max_allowed_packet
0
'#---------------------FN_DYNVARS_070_10----------------------#'
SET @@max_allowed_packet = 100000;
ERROR HY000: SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value
SELECT @@max_allowed_packet = @@local.max_allowed_packet;
@@max_allowed_packet = @@local.max_allowed_packet
1
SELECT @@local.max_allowed_packet = @@session.max_allowed_packet;
@@local.max_allowed_packet = @@session.max_allowed_packet
1
'#---------------------FN_DYNVARS_070_11----------------------#'
SET max_allowed_packet = 1024;
ERROR HY000: SESSION variable 'max_allowed_packet' is read-only. Use SET GLOBAL to assign the value
SELECT @@max_allowed_packet;
@@max_allowed_packet
67108864
SELECT local.max_allowed_packet;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.max_allowed_packet;
ERROR 42S02: Unknown table 'session' in field list
SELECT max_allowed_packet = @@session.max_allowed_packet;
ERROR 42S22: Unknown column 'max_allowed_packet' in 'field list'
SET @@global.max_allowed_packet = @start_global_value;
SELECT @@global.max_allowed_packet;
@@global.max_allowed_packet
67108864
SELECT @@session.max_allowed_packet;
@@session.max_allowed_packet
67108864
