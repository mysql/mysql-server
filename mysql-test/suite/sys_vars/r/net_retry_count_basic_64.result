SET @start_global_value = @@global.net_retry_count;
SELECT @start_global_value;
@start_global_value
10
SET @start_session_value = @@session.net_retry_count;
SELECT @start_session_value;
@start_session_value
10
'#--------------------FN_DYNVARS_111_01-------------------------#'
SET @@global.net_retry_count = 100;
SET @@global.net_retry_count = DEFAULT;
SELECT @@global.net_retry_count;
@@global.net_retry_count
10
SET @@session.net_retry_count = 200;
SET @@session.net_retry_count = DEFAULT;
SELECT @@session.net_retry_count;
@@session.net_retry_count
10
'#--------------------FN_DYNVARS_111_02-------------------------#'
SET @@global.net_retry_count = DEFAULT;
SELECT @@global.net_retry_count = 10;
@@global.net_retry_count = 10
1
SET @@session.net_retry_count = DEFAULT;
SELECT @@session.net_retry_count = 10;
@@session.net_retry_count = 10
1
'#--------------------FN_DYNVARS_111_03-------------------------#'
SET @@global.net_retry_count = 1;
SELECT @@global.net_retry_count;
@@global.net_retry_count
1
SET @@global.net_retry_count = 2;
SELECT @@global.net_retry_count;
@@global.net_retry_count
2
SET @@global.net_retry_count = 4294967295;
SELECT @@global.net_retry_count;
@@global.net_retry_count
4294967295
SET @@global.net_retry_count = 4294967294;
SELECT @@global.net_retry_count;
@@global.net_retry_count
4294967294
SET @@global.net_retry_count = 65536;
SELECT @@global.net_retry_count;
@@global.net_retry_count
65536
SET @@global.net_retry_count = 18446744073709551615;
SELECT @@global.net_retry_count;
@@global.net_retry_count
18446744073709551615
'#--------------------FN_DYNVARS_111_04-------------------------#'
SET @@session.net_retry_count = 1;
SELECT @@session.net_retry_count;
@@session.net_retry_count
1
SET @@session.net_retry_count = 2;
SELECT @@session.net_retry_count;
@@session.net_retry_count
2
SET @@session.net_retry_count = 65535;
SELECT @@session.net_retry_count;
@@session.net_retry_count
65535
SET @@session.net_retry_count = 4294967295;
SELECT @@session.net_retry_count;
@@session.net_retry_count
4294967295
SET @@session.net_retry_count = 4294967294;
SELECT @@session.net_retry_count;
@@session.net_retry_count
4294967294
SET @@session.net_retry_count = 18446744073709551615;
SELECT @@session.net_retry_count;
@@session.net_retry_count
18446744073709551615
'#------------------FN_DYNVARS_111_05-----------------------#'
SET @@global.net_retry_count = 0;
Warnings:
Warning	1292	Truncated incorrect net_retry_count value: '0'
SELECT @@global.net_retry_count;
@@global.net_retry_count
1
SET @@global.net_retry_count = -1024;
Warnings:
Warning	1292	Truncated incorrect net_retry_count value: '-1024'
SELECT @@global.net_retry_count;
@@global.net_retry_count
1
SET @@global.net_retry_count = 65530.34;
ERROR 42000: Incorrect argument type to variable 'net_retry_count'
SELECT @@global.net_retry_count;
@@global.net_retry_count
1
SET @@global.net_retry_count = test;
ERROR 42000: Incorrect argument type to variable 'net_retry_count'
SELECT @@global.net_retry_count;
@@global.net_retry_count
1
SET @@global.net_retry_count = 18446744073709551616;
ERROR 42000: Incorrect argument type to variable 'net_retry_count'
SELECT @@global.net_retry_count;
@@global.net_retry_count
1
SET @@session.net_retry_count = 0;
Warnings:
Warning	1292	Truncated incorrect net_retry_count value: '0'
SELECT @@session.net_retry_count;
@@session.net_retry_count
1
SET @@session.net_retry_count = -2;
Warnings:
Warning	1292	Truncated incorrect net_retry_count value: '-2'
SELECT @@session.net_retry_count;
@@session.net_retry_count
1
SET @@session.net_retry_count = 65530.34;
ERROR 42000: Incorrect argument type to variable 'net_retry_count'
SET @@session.net_retry_count = test;
ERROR 42000: Incorrect argument type to variable 'net_retry_count'
SELECT @@session.net_retry_count;
@@session.net_retry_count
1
SET @@session.net_retry_count = 18446744073709551616;
ERROR 42000: Incorrect argument type to variable 'net_retry_count'
SELECT @@session.net_retry_count;
@@session.net_retry_count
1
'#------------------FN_DYNVARS_111_06-----------------------#'
SELECT @@global.net_retry_count = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='net_retry_count';
@@global.net_retry_count = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_111_07-----------------------#'
SELECT @@session.net_retry_count = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='net_retry_count';
@@session.net_retry_count = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_111_08-----------------------#'
SET @@global.net_retry_count = TRUE;
SELECT @@global.net_retry_count;
@@global.net_retry_count
1
SET @@global.net_retry_count = FALSE;
Warnings:
Warning	1292	Truncated incorrect net_retry_count value: '0'
SELECT @@global.net_retry_count;
@@global.net_retry_count
1
'#---------------------FN_DYNVARS_111_09----------------------#'
SET @@global.net_retry_count = 10;
SELECT @@net_retry_count = @@global.net_retry_count;
@@net_retry_count = @@global.net_retry_count
0
'#---------------------FN_DYNVARS_111_10----------------------#'
SET @@net_retry_count = 100;
SELECT @@net_retry_count = @@local.net_retry_count;
@@net_retry_count = @@local.net_retry_count
1
SELECT @@local.net_retry_count = @@session.net_retry_count;
@@local.net_retry_count = @@session.net_retry_count
1
'#---------------------FN_DYNVARS_111_11----------------------#'
SET net_retry_count = 1;
SELECT @@net_retry_count;
@@net_retry_count
1
SELECT local.net_retry_count;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.net_retry_count;
ERROR 42S02: Unknown table 'session' in field list
SELECT net_retry_count = @@session.net_retry_count;
ERROR 42S22: Unknown column 'net_retry_count' in 'field list'
SET @@global.net_retry_count = @start_global_value;
SELECT @@global.net_retry_count;
@@global.net_retry_count
10
SET @@session.net_retry_count = @start_session_value;
SELECT @@session.net_retry_count;
@@session.net_retry_count
10
