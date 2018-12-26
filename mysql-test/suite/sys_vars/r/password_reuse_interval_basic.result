SET @password_reuse_interval_save = @@global.password_reuse_interval;
SELECT @password_reuse_interval_save;
@password_reuse_interval_save
0
Default value must be 0 for global scope
SET @@global.password_reuse_interval = 1;
SET @@global.password_reuse_interval = DEFAULT;
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
0
SET @@session.password_reuse_interval = 2;
ERROR HY000: Variable 'password_reuse_interval' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.password_reuse_interval = DEFAULT;
ERROR HY000: Variable 'password_reuse_interval' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@session.password_reuse_interval;
ERROR HY000: Variable 'password_reuse_interval' is a GLOBAL variable
SET @@global.password_reuse_interval = NULL;
ERROR 42000: Incorrect argument type to variable 'password_reuse_interval'
SET @@global.password_reuse_interval = '';
ERROR 42000: Incorrect argument type to variable 'password_reuse_interval'
SET @@global.password_reuse_interval = ' ';
ERROR 42000: Incorrect argument type to variable 'password_reuse_interval'
SET @@session.password_reuse_interval = NULL;
ERROR HY000: Variable 'password_reuse_interval' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.password_reuse_interval = '';
ERROR HY000: Variable 'password_reuse_interval' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.password_reuse_interval = ' ';
ERROR HY000: Variable 'password_reuse_interval' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global.password_reuse_interval = 0;
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
0
SET @@global.password_reuse_interval = 1;
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
1
SET @@global.password_reuse_interval = TRUE;
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
1
SET @@global.password_reuse_interval = FALSE;
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
0
SET @@global.password_reuse_interval = 4294967295;
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
4294967295
SET @@global.password_reuse_interval = DEFAULT;
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
0
SET @@global.password_reuse_interval = -1;
Warnings:
Warning	1292	Truncated incorrect password_reuse_interval value: '-1'
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
0
SET @@global.password_reuse_interval = 'ABC';
ERROR 42000: Incorrect argument type to variable 'password_reuse_interval'
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
0
SET @@global.password_reuse_interval = ON;
ERROR 42000: Incorrect argument type to variable 'password_reuse_interval'
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
0
SET @@global.password_reuse_interval = 'OFF';
ERROR 42000: Incorrect argument type to variable 'password_reuse_interval'
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
0
SET @@global.password_reuse_interval = 2.14;
ERROR 42000: Incorrect argument type to variable 'password_reuse_interval'
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
0
SET @@global.password_reuse_interval = NONE;
ERROR 42000: Incorrect argument type to variable 'password_reuse_interval'
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
0
SET @@global.password_reuse_interval = -4294967295;
Warnings:
Warning	1292	Truncated incorrect password_reuse_interval value: '-4294967295'
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
0
SET @@global.password_reuse_interval = 4294967296;
Warnings:
Warning	1292	Truncated incorrect password_reuse_interval value: '4294967296'
SELECT @@global.password_reuse_interval;
@@global.password_reuse_interval
4294967295
Must return 1
SELECT @@global.password_reuse_interval = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='password_reuse_interval';
@@global.password_reuse_interval = VARIABLE_VALUE
1
SET GLOBAL password_reuse_interval = 1;
SET @@global.password_reuse_interval = 1;
SET global.password_reuse_interval = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.password_reuse_interval = 1' at line 1
SET @@password_reuse_interval = 1;
ERROR HY000: Variable 'password_reuse_interval' is a GLOBAL variable and should be set with SET GLOBAL
SET password_reuse_interval = 1;
ERROR HY000: Variable 'password_reuse_interval' is a GLOBAL variable and should be set with SET GLOBAL
SELECT global.password_reuse_interval;
ERROR 42S02: Unknown table 'global' in field list
SELECT GLOBAL password_reuse_interval;
ERROR 42S22: Unknown column 'GLOBAL' in 'field list'
SELECT @@password_reuse_interval;
@@password_reuse_interval
1
SET @@global.password_reuse_interval = @password_reuse_interval_save;
