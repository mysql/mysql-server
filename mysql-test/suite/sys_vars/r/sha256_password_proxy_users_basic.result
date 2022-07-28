SET @start_value = @@global.sha256_password_proxy_users;
SELECT @start_value;
@start_value
0
'#----- 3.2.1 Default value -----------------------------------#'
SET @@global.sha256_password_proxy_users = 1;
SET @@global.sha256_password_proxy_users = DEFAULT;
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
SET @@global.sha256_password_proxy_users = @start_value;
SELECT @@global.sha256_password_proxy_users = 0;
@@global.sha256_password_proxy_users = 0
1
'#----- 3.2.1 Valid values ------------------------------------#'
SET @@global.sha256_password_proxy_users = 0;
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
SET @@global.sha256_password_proxy_users = 1;
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
1
SET @@global.sha256_password_proxy_users = TRUE;
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
1
SET @@global.sha256_password_proxy_users = FALSE;
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
SET @@global.sha256_password_proxy_users = ON;
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
1
SET @@global.sha256_password_proxy_users = OFF;
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
'#----- 3.2.1 Invalid values -----------------------------------#'
SET @@global.sha256_password_proxy_users = -1;
ERROR 42000: Variable 'sha256_password_proxy_users' can't be set to the value of '-1'
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
SET @@global.sha256_password_proxy_users = 4294967296;
ERROR 42000: Variable 'sha256_password_proxy_users' can't be set to the value of '4294967296'
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
SET @@global.sha256_password_proxy_users = 10240022115;
ERROR 42000: Variable 'sha256_password_proxy_users' can't be set to the value of '10240022115'
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
SET @@global.sha256_password_proxy_users = 10000.01;
ERROR 42000: Incorrect argument type to variable 'sha256_password_proxy_users'
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
SET @@global.sha256_password_proxy_users = -1024;
ERROR 42000: Variable 'sha256_password_proxy_users' can't be set to the value of '-1024'
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
SET @@global.sha256_password_proxy_users = 42949672950;
ERROR 42000: Variable 'sha256_password_proxy_users' can't be set to the value of '42949672950'
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
SET @@global.sha256_password_proxy_users = 'test';
ERROR 42000: Variable 'sha256_password_proxy_users' can't be set to the value of 'test'
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
'#----- 3.2.5 Session variable -----------------------------------#'
SET @@session.sha256_password_proxy_users = 0;
ERROR HY000: Variable 'sha256_password_proxy_users' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@sha256_password_proxy_users;
@@sha256_password_proxy_users
0
SELECT IF(@@global.sha256_password_proxy_users, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='sha256_password_proxy_users';
IF(@@global.sha256_password_proxy_users, "ON", "OFF") = VARIABLE_VALUE
1
SELECT IF(@@sha256_password_proxy_users, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='sha256_password_proxy_users';
IF(@@sha256_password_proxy_users, "ON", "OFF") = VARIABLE_VALUE
1
SET @@global.sha256_password_proxy_users = 1;
SELECT @@sha256_password_proxy_users = @@global.sha256_password_proxy_users;
@@sha256_password_proxy_users = @@global.sha256_password_proxy_users
1
'#----- 3.2.6 Notation with/without @@ sign ------------------#'
SET sha256_password_proxy_users = 1;
ERROR HY000: Variable 'sha256_password_proxy_users' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@sha256_password_proxy_users;
@@sha256_password_proxy_users
1
SET local.sha256_password_proxy_users = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.sha256_password_proxy_users = 1' at line 1
SELECT local.sha256_password_proxy_users;
ERROR 42S02: Unknown table 'local' in field list
SET global.sha256_password_proxy_users = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.sha256_password_proxy_users = 1' at line 1
SELECT global.sha256_password_proxy_users;
ERROR 42S02: Unknown table 'global' in field list
SELECT sha256_password_proxy_users;
ERROR 42S22: Unknown column 'sha256_password_proxy_users' in 'field list'
SELECT @@session.sha256_password_proxy_users;
ERROR HY000: Variable 'sha256_password_proxy_users' is a GLOBAL variable
SET @@global.sha256_password_proxy_users = @start_value;
SELECT @@global.sha256_password_proxy_users;
@@global.sha256_password_proxy_users
0
