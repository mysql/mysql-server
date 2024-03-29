SET @start_value = @@global.check_proxy_users;
SELECT @start_value;
@start_value
0
'#----- 1.2.2.3 Default ---------------------------------------#'
SET @@global.check_proxy_users = 1;
SET @@global.check_proxy_users = DEFAULT;
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
SET @@global.check_proxy_users = @start_value;
SELECT @@global.check_proxy_users = 0;
@@global.check_proxy_users = 0
1
'#----- 1.2.1 Valid values -------------------------------------#'
SET @@global.check_proxy_users = 0;
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
SET @@global.check_proxy_users = 1;
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
1
SET @@global.check_proxy_users = TRUE;
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
1
SET @@global.check_proxy_users = FALSE;
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
SET @@global.check_proxy_users = ON;
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
1
SET @@global.check_proxy_users = OFF;
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
'#----- 1.2.1 Invalid values -----------------------------------#'
SET @@global.check_proxy_users = -1;
ERROR 42000: Variable 'check_proxy_users' can't be set to the value of '-1'
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
SET @@global.check_proxy_users = 4294967296;
ERROR 42000: Variable 'check_proxy_users' can't be set to the value of '4294967296'
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
SET @@global.check_proxy_users = 10240022115;
ERROR 42000: Variable 'check_proxy_users' can't be set to the value of '10240022115'
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
SET @@global.check_proxy_users = 10000.01;
ERROR 42000: Incorrect argument type to variable 'check_proxy_users'
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
SET @@global.check_proxy_users = -1024;
ERROR 42000: Variable 'check_proxy_users' can't be set to the value of '-1024'
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
SET @@global.check_proxy_users = 42949672950;
ERROR 42000: Variable 'check_proxy_users' can't be set to the value of '42949672950'
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
SET @@global.check_proxy_users = 'test';
ERROR 42000: Variable 'check_proxy_users' can't be set to the value of 'test'
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
'#----- 1.2.5 Global/Session variable ---------------------------#'
SET @@session.check_proxy_users = 0;
ERROR HY000: Variable 'check_proxy_users' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@check_proxy_users;
@@check_proxy_users
0
SELECT IF(@@global.check_proxy_users, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='check_proxy_users';
IF(@@global.check_proxy_users, "ON", "OFF") = VARIABLE_VALUE
1
SELECT IF(@@check_proxy_users, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='check_proxy_users';
IF(@@check_proxy_users, "ON", "OFF") = VARIABLE_VALUE
1
SET @@global.check_proxy_users = 1;
SELECT @@check_proxy_users = @@global.check_proxy_users;
@@check_proxy_users = @@global.check_proxy_users
1
'#----- 1.2.6 Global/Session variable ---------------------------#'
SET check_proxy_users = 1;
ERROR HY000: Variable 'check_proxy_users' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@check_proxy_users;
@@check_proxy_users
1
SET local.check_proxy_users = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.check_proxy_users = 1' at line 1
SELECT local.check_proxy_users;
ERROR 42S02: Unknown table 'local' in field list
SET global.check_proxy_users = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.check_proxy_users = 1' at line 1
SELECT global.check_proxy_users;
ERROR 42S02: Unknown table 'global' in field list
SELECT check_proxy_users;
ERROR 42S22: Unknown column 'check_proxy_users' in 'field list'
SELECT @@session.check_proxy_users;
ERROR HY000: Variable 'check_proxy_users' is a GLOBAL variable
SET @@global.check_proxy_users = @start_value;
SELECT @@global.check_proxy_users;
@@global.check_proxy_users
0
