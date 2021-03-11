SET @start_value = @@global.mysql_native_password_proxy_users;
SELECT @start_value;
@start_value
0
'#----- 2.2.1 Default value -----------------------------------#'
SET @@global.mysql_native_password_proxy_users = 1;
SET @@global.mysql_native_password_proxy_users = DEFAULT;
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
0
SET @@global.mysql_native_password_proxy_users = @start_value;
SELECT @@global.mysql_native_password_proxy_users = 0;
@@global.mysql_native_password_proxy_users = 0
1
'#----- 2.2.1 Valid values ------------------------------------#'
SET @@global.mysql_native_password_proxy_users = 0;
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
0
SET @@global.mysql_native_password_proxy_users = 1;
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
1
SET @@global.mysql_native_password_proxy_users = TRUE;
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
1
SET @@global.mysql_native_password_proxy_users = FALSE;
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
0
SET @@global.mysql_native_password_proxy_users = ON;
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
1
SET @@global.mysql_native_password_proxy_users = OFF;
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
0
'#----- 2.2.1 Invalid values -----------------------------------#'
SET @@global.mysql_native_password_proxy_users = -1;
ERROR 42000: Variable 'mysql_native_password_proxy_users' can't be set to the value of '-1'
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
0
SET @@global.mysql_native_password_proxy_users = 4294967296;
ERROR 42000: Variable 'mysql_native_password_proxy_users' can't be set to the value of '4294967296'
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
0
SET @@global.mysql_native_password_proxy_users = 10240022115;
ERROR 42000: Variable 'mysql_native_password_proxy_users' can't be set to the value of '10240022115'
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
0
SET @@global.mysql_native_password_proxy_users = 10000.01;
ERROR 42000: Incorrect argument type to variable 'mysql_native_password_proxy_users'
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
0
SET @@global.mysql_native_password_proxy_users = -1024;
ERROR 42000: Variable 'mysql_native_password_proxy_users' can't be set to the value of '-1024'
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
0
SET @@global.mysql_native_password_proxy_users = 42949672950;
ERROR 42000: Variable 'mysql_native_password_proxy_users' can't be set to the value of '42949672950'
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
0
SET @@global.mysql_native_password_proxy_users = 'test';
ERROR 42000: Variable 'mysql_native_password_proxy_users' can't be set to the value of 'test'
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
0
'#----- 2.2.5 Session variable -----------------------------------#'
SET @@session.mysql_native_password_proxy_users = 0;
ERROR HY000: Variable 'mysql_native_password_proxy_users' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@mysql_native_password_proxy_users;
@@mysql_native_password_proxy_users
0
SELECT IF(@@global.mysql_native_password_proxy_users, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='mysql_native_password_proxy_users';
IF(@@global.mysql_native_password_proxy_users, "ON", "OFF") = VARIABLE_VALUE
1
SELECT IF(@@mysql_native_password_proxy_users, "ON", "OFF") = VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='mysql_native_password_proxy_users';
IF(@@mysql_native_password_proxy_users, "ON", "OFF") = VARIABLE_VALUE
1
SET @@global.mysql_native_password_proxy_users = 1;
SELECT @@mysql_native_password_proxy_users = @@global.mysql_native_password_proxy_users;
@@mysql_native_password_proxy_users = @@global.mysql_native_password_proxy_users
1
'#----- 2.2.6 Notation with/without @@ sign --------------#'
SET mysql_native_password_proxy_users = 1;
ERROR HY000: Variable 'mysql_native_password_proxy_users' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@mysql_native_password_proxy_users;
@@mysql_native_password_proxy_users
1
SET local.mysql_native_password_proxy_users = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'local.mysql_native_password_proxy_users = 1' at line 1
SELECT local.mysql_native_password_proxy_users;
ERROR 42S02: Unknown table 'local' in field list
SET global.mysql_native_password_proxy_users = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.mysql_native_password_proxy_users = 1' at line 1
SELECT global.mysql_native_password_proxy_users;
ERROR 42S02: Unknown table 'global' in field list
SELECT mysql_native_password_proxy_users;
ERROR 42S22: Unknown column 'mysql_native_password_proxy_users' in 'field list'
SELECT @@session.mysql_native_password_proxy_users;
ERROR HY000: Variable 'mysql_native_password_proxy_users' is a GLOBAL variable
SET @@global.mysql_native_password_proxy_users = @start_value;
SELECT @@global.mysql_native_password_proxy_users;
@@global.mysql_native_password_proxy_users
0
