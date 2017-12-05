SET @password_history_save = @@global.password_history;
SELECT @password_history_save;
@password_history_save
0
Default value must be 0 for global scope
SET @@global.password_history = 1;
SET @@global.password_history = DEFAULT;
SELECT @@global.password_history;
@@global.password_history
0
SET @@session.password_history = 2;
ERROR HY000: Variable 'password_history' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.password_history = DEFAULT;
ERROR HY000: Variable 'password_history' is a GLOBAL variable and should be set with SET GLOBAL
SELECT @@session.password_history;
ERROR HY000: Variable 'password_history' is a GLOBAL variable
SET @@global.password_history = NULL;
ERROR 42000: Incorrect argument type to variable 'password_history'
SET @@global.password_history = '';
ERROR 42000: Incorrect argument type to variable 'password_history'
SET @@global.password_history = ' ';
ERROR 42000: Incorrect argument type to variable 'password_history'
SET @@session.password_history = NULL;
ERROR HY000: Variable 'password_history' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.password_history = '';
ERROR HY000: Variable 'password_history' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.password_history = ' ';
ERROR HY000: Variable 'password_history' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global.password_history = 0;
SELECT @@global.password_history;
@@global.password_history
0
SET @@global.password_history = 1;
SELECT @@global.password_history;
@@global.password_history
1
SET @@global.password_history = TRUE;
SELECT @@global.password_history;
@@global.password_history
1
SET @@global.password_history = FALSE;
SELECT @@global.password_history;
@@global.password_history
0
SET @@global.password_history = 4294967295;
SELECT @@global.password_history;
@@global.password_history
4294967295
SET @@global.password_history = DEFAULT;
SELECT @@global.password_history;
@@global.password_history
0
SET @@global.password_history = -1;
Warnings:
Warning	1292	Truncated incorrect password_history value: '-1'
SELECT @@global.password_history;
@@global.password_history
0
SET @@global.password_history = 'ABC';
ERROR 42000: Incorrect argument type to variable 'password_history'
SELECT @@global.password_history;
@@global.password_history
0
SET @@global.password_history = ON;
ERROR 42000: Incorrect argument type to variable 'password_history'
SELECT @@global.password_history;
@@global.password_history
0
SET @@global.password_history = 'OFF';
ERROR 42000: Incorrect argument type to variable 'password_history'
SELECT @@global.password_history;
@@global.password_history
0
SET @@global.password_history = 2.14;
ERROR 42000: Incorrect argument type to variable 'password_history'
SELECT @@global.password_history;
@@global.password_history
0
SET @@global.password_history = NONE;
ERROR 42000: Incorrect argument type to variable 'password_history'
SELECT @@global.password_history;
@@global.password_history
0
SET @@global.password_history = -4294967295;
Warnings:
Warning	1292	Truncated incorrect password_history value: '-4294967295'
SELECT @@global.password_history;
@@global.password_history
0
SET @@global.password_history = 4294967296;
Warnings:
Warning	1292	Truncated incorrect password_history value: '4294967296'
SELECT @@global.password_history;
@@global.password_history
4294967295
Must return 1
SELECT @@global.password_history = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='password_history';
@@global.password_history = VARIABLE_VALUE
1
SET GLOBAL password_history = 1;
SET @@global.password_history = 1;
SET global.password_history = 1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.password_history = 1' at line 1
SET @@password_history = 1;
ERROR HY000: Variable 'password_history' is a GLOBAL variable and should be set with SET GLOBAL
SET password_history = 1;
ERROR HY000: Variable 'password_history' is a GLOBAL variable and should be set with SET GLOBAL
SELECT global.password_history;
ERROR 42S02: Unknown table 'global' in field list
SELECT GLOBAL password_history;
ERROR 42S22: Unknown column 'GLOBAL' in 'field list'
SELECT @@password_history;
@@password_history
1
SET @@global.password_history = @password_history_save;
