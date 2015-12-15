'#--------------------FN_DYNVARS_165_01-------------------------#'
SET @start_global_value = @@global.sql_slave_skip_counter;
SELECT @@global.sql_slave_skip_counter;
@@global.sql_slave_skip_counter
0
'#--------------------FN_DYNVARS_165_02-------------------------#'
SET @@global.sql_slave_skip_counter = DEFAULT;
SELECT @@global.sql_slave_skip_counter;
@@global.sql_slave_skip_counter
0
'#--------------------FN_DYNVARS_165_03-------------------------#'
SET @@global.sql_slave_skip_counter = 0;
SET @@global.sql_slave_skip_counter = 5;
SET @@global.sql_slave_skip_counter = 1024;
SET @@global.sql_slave_skip_counter = 2147483648;
SET @@global.sql_slave_skip_counter = 2147483648*2;
Warnings:
Warning	1292	Truncated incorrect sql_slave_skip_counter value: '4294967296'
SET @@global.sql_slave_skip_counter = 2147483648*2-1;
SET @@global.sql_slave_skip_counter = 18446744065119617025;
Warnings:
Warning	1292	Truncated incorrect sql_slave_skip_counter value: '18446744065119617025'
'#--------------------FN_DYNVARS_165_03-------------------------#'
SET @@global.sql_slave_skip_counter = '5';
ERROR 42000: Incorrect argument type to variable 'sql_slave_skip_counter'
SET @@global.sql_slave_skip_counter = 5.5;
ERROR 42000: Incorrect argument type to variable 'sql_slave_skip_counter'
SET @@global.sql_slave_skip_counter = -.5;
ERROR 42000: Incorrect argument type to variable 'sql_slave_skip_counter'
SET @@global.sql_slave_skip_counter = -.0;
ERROR 42000: Incorrect argument type to variable 'sql_slave_skip_counter'
'#--------------------FN_DYNVARS_165_03-------------------------#'
SET @@global.sql_slave_skip_counter = 1024;
SELECT VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='sql_slave_skip_counter';
VARIABLE_VALUE
1024
'#--------------------FN_DYNVARS_165_03-------------------------#'
SET @@sql_slave_skip_counter = 10;
ERROR HY000: Variable 'sql_slave_skip_counter' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.sql_slave_skip_counter = 12;
ERROR HY000: Variable 'sql_slave_skip_counter' is a GLOBAL variable and should be set with SET GLOBAL
SET @@local.sql_slave_skip_counter = 13;
ERROR HY000: Variable 'sql_slave_skip_counter' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global.sql_slave_skip_counter = 0;
SET @@global.enforce_gtid_consistency = on;
SET @@global.gtid_mode = off_permissive;
set global sql_slave_skip_counter= 1;
set global sql_slave_skip_counter= 0;
SET @@global.gtid_mode = on_permissive;
set global sql_slave_skip_counter= 1;
set global sql_slave_skip_counter= 0;
SET @@global.gtid_mode = on;
set global sql_slave_skip_counter= 1;
ERROR HY000: sql_slave_skip_counter can not be set when the server is running with @@GLOBAL.GTID_MODE = ON. Instead, for each transaction that you want to skip, generate an empty transaction with the same GTID as the transaction
select @@global.sql_slave_skip_counter;
@@global.sql_slave_skip_counter
0
set global sql_slave_skip_counter= 0;
ERROR HY000: sql_slave_skip_counter can not be set when the server is running with @@GLOBAL.GTID_MODE = ON. Instead, for each transaction that you want to skip, generate an empty transaction with the same GTID as the transaction
select @@global.sql_slave_skip_counter;
@@global.sql_slave_skip_counter
0
SET @@global.gtid_mode = on_permissive;
SET @@global.gtid_mode = off_permissive;
SET @@global.gtid_mode = off;
SET @@global.enforce_gtid_consistency = off;
