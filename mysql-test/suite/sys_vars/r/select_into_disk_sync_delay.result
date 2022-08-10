SET @start_global_value = @@global.select_into_disk_sync_delay;
SELECT @start_global_value;
@start_global_value
0
SET @start_session_value = @@session.select_into_disk_sync_delay;
SELECT @start_session_value;
@start_session_value
0
'#--------------------FN_DYNVARS_002_01-------------------------#'
SET @@global.select_into_disk_sync_delay = 1000;
SET @@global.select_into_disk_sync_delay = DEFAULT;
SELECT @@global.select_into_disk_sync_delay;
@@global.select_into_disk_sync_delay
0
SET @@session.select_into_disk_sync_delay = 3000;
SET @@session.select_into_disk_sync_delay = DEFAULT;
SELECT @@session.select_into_disk_sync_delay;
@@session.select_into_disk_sync_delay
0
'#--------------------FN_DYNVARS_002_02-------------------------#'
SET @@global.select_into_disk_sync_delay = @start_global_value;
SELECT @@global.select_into_disk_sync_delay = 10;
@@global.select_into_disk_sync_delay = 10
0
SET @@session.select_into_disk_sync_delay = @start_session_value;
SELECT @@session.select_into_disk_sync_delay = 20;
@@session.select_into_disk_sync_delay = 20
0
'#--------------------FN_DYNVARS_002_03-------------------------#'
SET @@global.select_into_disk_sync_delay = 5;
SELECT @@global.select_into_disk_sync_delay;
@@global.select_into_disk_sync_delay
5
SET @@global.select_into_disk_sync_delay = 12;
SELECT @@global.select_into_disk_sync_delay;
@@global.select_into_disk_sync_delay
12
SET @@global.select_into_disk_sync_delay = 100;
SELECT @@global.select_into_disk_sync_delay;
@@global.select_into_disk_sync_delay
100
'#--------------------FN_DYNVARS_002_04-------------------------#'
SET @@session.select_into_disk_sync_delay = 4;
SELECT @@session.select_into_disk_sync_delay;
@@session.select_into_disk_sync_delay
4
SET @@session.select_into_disk_sync_delay = 8;
SELECT @@session.select_into_disk_sync_delay;
@@session.select_into_disk_sync_delay
8
SET @@session.select_into_disk_sync_delay = 17;
SELECT @@session.select_into_disk_sync_delay;
@@session.select_into_disk_sync_delay
17
'#------------------FN_DYNVARS_002_05-----------------------#'
SET @@global.select_into_disk_sync_delay = -1;
Warnings:
Warning	1292	Truncated incorrect select_into_disk_sync_delay value: '-1'
SELECT @@global.select_into_disk_sync_delay;
@@global.select_into_disk_sync_delay
0
SET @@global.select_into_disk_sync_delay = -1024;
Warnings:
Warning	1292	Truncated incorrect select_into_disk_sync_delay value: '-1024'
SELECT @@global.select_into_disk_sync_delay;
@@global.select_into_disk_sync_delay
0
SET @@global.select_into_disk_sync_delay = 3747474747474747474;
Warnings:
Warning	1292	Truncated incorrect select_into_disk_sync_delay value: '3747474747474747474'
SELECT @@global.select_into_disk_sync_delay;
@@global.select_into_disk_sync_delay
31536000
SET @@global.select_into_disk_sync_delay = ON;
ERROR 42000: Incorrect argument type to variable 'select_into_disk_sync_delay'
SELECT @@global.select_into_disk_sync_delay;
@@global.select_into_disk_sync_delay
31536000
SET @@global.select_into_disk_sync_delay = OFF;
ERROR 42000: Incorrect argument type to variable 'select_into_disk_sync_delay'
SELECT @@global.select_into_disk_sync_delay;
@@global.select_into_disk_sync_delay
31536000
SET @@global.select_into_disk_sync_delay = test;
ERROR 42000: Incorrect argument type to variable 'select_into_disk_sync_delay'
SELECT @@global.select_into_disk_sync_delay;
@@global.select_into_disk_sync_delay
31536000
SET @@session.select_into_disk_sync_delay = -1;
Warnings:
Warning	1292	Truncated incorrect select_into_disk_sync_delay value: '-1'
SELECT @@session.select_into_disk_sync_delay;
@@session.select_into_disk_sync_delay
0
SET @@session.select_into_disk_sync_delay = -2;
Warnings:
Warning	1292	Truncated incorrect select_into_disk_sync_delay value: '-2'
SELECT @@session.select_into_disk_sync_delay;
@@session.select_into_disk_sync_delay
0
SET @@session.select_into_disk_sync_delay = ON;
ERROR 42000: Incorrect argument type to variable 'select_into_disk_sync_delay'
SELECT @@session.select_into_disk_sync_delay;
@@session.select_into_disk_sync_delay
0
SET @@session.select_into_disk_sync_delay = OFF;
ERROR 42000: Incorrect argument type to variable 'select_into_disk_sync_delay'
SELECT @@session.select_into_disk_sync_delay;
@@session.select_into_disk_sync_delay
0
SET @@session.select_into_disk_sync_delay = test;
ERROR 42000: Incorrect argument type to variable 'select_into_disk_sync_delay'
SELECT @@session.select_into_disk_sync_delay;
@@session.select_into_disk_sync_delay
0
SET @@session.select_into_disk_sync_delay = 85858585858585858585;
ERROR 42000: Incorrect argument type to variable 'select_into_disk_sync_delay'
SELECT @@session.select_into_disk_sync_delay;
@@session.select_into_disk_sync_delay
0
'#------------------FN_DYNVARS_002_06-----------------------#'
SELECT @@global.select_into_disk_sync_delay = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='select_into_disk_sync_delay';
@@global.select_into_disk_sync_delay = VARIABLE_VALUE
1
'#------------------FN_DYNVARS_002_07-----------------------#'
SELECT @@session.select_into_disk_sync_delay = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='select_into_disk_sync_delay';
@@session.select_into_disk_sync_delay = VARIABLE_VALUE
1
'#---------------------FN_DYNVARS_001_09----------------------#'
SET @@global.select_into_disk_sync_delay = 10;
SET @@session.select_into_disk_sync_delay = 1000;
SELECT @@select_into_disk_sync_delay = @@global.select_into_disk_sync_delay;
@@select_into_disk_sync_delay = @@global.select_into_disk_sync_delay
0
'#---------------------FN_DYNVARS_001_10----------------------#'
SET @@select_into_disk_sync_delay = 1000;
SELECT @@select_into_disk_sync_delay = @@local.select_into_disk_sync_delay;
@@select_into_disk_sync_delay = @@local.select_into_disk_sync_delay
1
SELECT @@local.select_into_disk_sync_delay = @@session.select_into_disk_sync_delay;
@@local.select_into_disk_sync_delay = @@session.select_into_disk_sync_delay
1
'#---------------------FN_DYNVARS_001_11----------------------#'
SET select_into_disk_sync_delay = 1000;
SELECT @@select_into_disk_sync_delay;
@@select_into_disk_sync_delay
1000
SELECT local.select_into_disk_sync_delay;
ERROR 42S02: Unknown table 'local' in field list
SELECT session.select_into_disk_sync_delay;
ERROR 42S02: Unknown table 'session' in field list
SELECT select_into_disk_sync_delay = @@session.select_into_disk_sync_delay;
ERROR 42S22: Unknown column 'select_into_disk_sync_delay' in 'field list'
'#---------------------FN_DYNVARS_001_12----------------------#'
CREATE USER foo, bar;
GRANT SESSION_VARIABLES_ADMIN ON *.*  TO bar;
SELECT @@session.select_into_disk_sync_delay;
@@session.select_into_disk_sync_delay
10
SET @@session.select_into_disk_sync_delay = 2000;
ERROR 42000: Access denied; you need (at least one of) the SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege(s) for this operation
SELECT @@session.select_into_disk_sync_delay;
@@session.select_into_disk_sync_delay
10
SET @@session.select_into_disk_sync_delay = 2000;
SET @@global.select_into_disk_sync_delay = 2000;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SELECT @@session.select_into_disk_sync_delay;
@@session.select_into_disk_sync_delay
2000
DROP USER foo, bar;
SET @@global.select_into_disk_sync_delay = @start_global_value;
SELECT @@global.select_into_disk_sync_delay;
@@global.select_into_disk_sync_delay
0
SET @@session.select_into_disk_sync_delay = @start_session_value;
SELECT @@session.select_into_disk_sync_delay;
@@session.select_into_disk_sync_delay
0
