SET @start_value= @@global.log_slow_admin_statements;
SET @@global.log_slow_admin_statements= DEFAULT;
SELECT @@global.log_slow_admin_statements;
@@global.log_slow_admin_statements
0
SET @@global.log_slow_admin_statements= TRUE;
SELECT @@global.log_slow_admin_statements;
@@global.log_slow_admin_statements
1
SET @@global.log_slow_admin_statements= true;
SELECT @@global.log_slow_admin_statements;
@@global.log_slow_admin_statements
1
SET @@global.log_slow_admin_statements= 0;
SELECT @@global.log_slow_admin_statements;
@@global.log_slow_admin_statements
0
SET @@global.log_slow_admin_statements= 1;
SELECT @@global.log_slow_admin_statements;
@@global.log_slow_admin_statements
1
SET @goodvar= TRUE;
SET @@global.log_slow_admin_statements= @goodvar;
SELECT @@global.log_slow_admin_statements;
@@global.log_slow_admin_statements
1
SET GLOBAL log_slow_admin_statements= DEFAULT;
SELECT @@global.log_slow_admin_statements;
@@global.log_slow_admin_statements
0
SET GLOBAL log_slow_admin_statements= ON;
SELECT @@global.log_slow_admin_statements;
@@global.log_slow_admin_statements
1
SET GLOBAL log_slow_admin_statements= OFF;
SELECT @@global.log_slow_admin_statements;
@@global.log_slow_admin_statements
0
SET GLOBAL log_slow_admin_statements= -0;
SELECT @@global.log_slow_admin_statements;
@@global.log_slow_admin_statements
0
SET GLOBAL log_slow_admin_statements= 0.00;
ERROR 42000: Incorrect argument type to variable 'log_slow_admin_statements'
SET GLOBAL log_slow_admin_statements= +0;
SELECT @@global.log_slow_admin_statements;
@@global.log_slow_admin_statements
0
SET @@global.log_slow_admin_statements= 'DEFAULT';
ERROR 42000: Variable 'log_slow_admin_statements' can't be set to the value of 'DEFAULT'
SET @@global.log_slow_admin_statements= 'true';
ERROR 42000: Variable 'log_slow_admin_statements' can't be set to the value of 'true'
SET @@global.log_slow_admin_statements= BLABLA;
ERROR 42000: Variable 'log_slow_admin_statements' can't be set to the value of 'BLABLA'
SET @@global.log_slow_admin_statements= 25;
ERROR 42000: Variable 'log_slow_admin_statements' can't be set to the value of '25'
SET GLOBAL log_slow_admin_statements= -1;
ERROR 42000: Variable 'log_slow_admin_statements' can't be set to the value of '-1'
SET @badvar= 'true';
SET @@global.log_slow_admin_statements= @badvar;
ERROR 42000: Variable 'log_slow_admin_statements' can't be set to the value of 'true'
SET GLOBAL log_slow_admin_statements= 'DEFAULT';
ERROR 42000: Variable 'log_slow_admin_statements' can't be set to the value of 'DEFAULT'
SET log_slow_admin_statements= TRUE;
ERROR HY000: Variable 'log_slow_admin_statements' is a GLOBAL variable and should be set with SET GLOBAL
SET SESSION log_slow_admin_statements= TRUE;
ERROR HY000: Variable 'log_slow_admin_statements' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.log_slow_admin_statements= TRUE;
ERROR HY000: Variable 'log_slow_admin_statements' is a GLOBAL variable and should be set with SET GLOBAL
SET LOCAL log_slow_admin_statements= TRUE;
ERROR HY000: Variable 'log_slow_admin_statements' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global log_slow_admin_statements= TRUE;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'log_slow_admin_statements= TRUE' at line 1
SET @@SESSION log_slow_admin_statements= TRUE;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'log_slow_admin_statements= TRUE' at line 1
SET @@global.log_slow_admin_statements= @start_value;
