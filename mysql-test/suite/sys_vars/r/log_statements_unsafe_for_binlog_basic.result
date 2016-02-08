SET @start_value= @@global.log_statements_unsafe_for_binlog;
SET @@global.log_statements_unsafe_for_binlog= DEFAULT;
SELECT @@global.log_statements_unsafe_for_binlog;
@@global.log_statements_unsafe_for_binlog
1
SET @@global.log_statements_unsafe_for_binlog= TRUE;
SELECT @@global.log_statements_unsafe_for_binlog;
@@global.log_statements_unsafe_for_binlog
1
SET @@global.log_statements_unsafe_for_binlog= true;
SELECT @@global.log_statements_unsafe_for_binlog;
@@global.log_statements_unsafe_for_binlog
1
SET @@global.log_statements_unsafe_for_binlog= 0;
SELECT @@global.log_statements_unsafe_for_binlog;
@@global.log_statements_unsafe_for_binlog
0
SET @@global.log_statements_unsafe_for_binlog= 1;
SELECT @@global.log_statements_unsafe_for_binlog;
@@global.log_statements_unsafe_for_binlog
1
SET @goodvar= TRUE;
SET @@global.log_statements_unsafe_for_binlog= @goodvar;
SELECT @@global.log_statements_unsafe_for_binlog;
@@global.log_statements_unsafe_for_binlog
1
SET GLOBAL log_statements_unsafe_for_binlog= DEFAULT;
SELECT @@global.log_statements_unsafe_for_binlog;
@@global.log_statements_unsafe_for_binlog
1
SET GLOBAL log_statements_unsafe_for_binlog= ON;
SELECT @@global.log_statements_unsafe_for_binlog;
@@global.log_statements_unsafe_for_binlog
1
SET GLOBAL log_statements_unsafe_for_binlog= OFF;
SELECT @@global.log_statements_unsafe_for_binlog;
@@global.log_statements_unsafe_for_binlog
0
SET GLOBAL log_statements_unsafe_for_binlog= -0;
SELECT @@global.log_statements_unsafe_for_binlog;
@@global.log_statements_unsafe_for_binlog
0
SET GLOBAL log_statements_unsafe_for_binlog= 0.00;
ERROR 42000: Incorrect argument type to variable 'log_statements_unsafe_for_binlog'
SET GLOBAL log_statements_unsafe_for_binlog= +0;
SELECT @@global.log_statements_unsafe_for_binlog;
@@global.log_statements_unsafe_for_binlog
0
SET @@global.log_statements_unsafe_for_binlog= 'DEFAULT';
ERROR 42000: Variable 'log_statements_unsafe_for_binlog' can't be set to the value of 'DEFAULT'
SET @@global.log_statements_unsafe_for_binlog= 'true';
ERROR 42000: Variable 'log_statements_unsafe_for_binlog' can't be set to the value of 'true'
SET @@global.log_statements_unsafe_for_binlog= BLABLA;
ERROR 42000: Variable 'log_statements_unsafe_for_binlog' can't be set to the value of 'BLABLA'
SET @@global.log_statements_unsafe_for_binlog= 25;
ERROR 42000: Variable 'log_statements_unsafe_for_binlog' can't be set to the value of '25'
SET GLOBAL log_statements_unsafe_for_binlog= -1;
ERROR 42000: Variable 'log_statements_unsafe_for_binlog' can't be set to the value of '-1'
SET @badvar= 'true';
SET @@global.log_statements_unsafe_for_binlog= @badvar;
ERROR 42000: Variable 'log_statements_unsafe_for_binlog' can't be set to the value of 'true'
SET GLOBAL log_statements_unsafe_for_binlog= 'DEFAULT';
ERROR 42000: Variable 'log_statements_unsafe_for_binlog' can't be set to the value of 'DEFAULT'
SET log_statements_unsafe_for_binlog= TRUE;
ERROR HY000: Variable 'log_statements_unsafe_for_binlog' is a GLOBAL variable and should be set with SET GLOBAL
SET SESSION log_statements_unsafe_for_binlog= TRUE;
ERROR HY000: Variable 'log_statements_unsafe_for_binlog' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.log_statements_unsafe_for_binlog= TRUE;
ERROR HY000: Variable 'log_statements_unsafe_for_binlog' is a GLOBAL variable and should be set with SET GLOBAL
SET LOCAL log_statements_unsafe_for_binlog= TRUE;
ERROR HY000: Variable 'log_statements_unsafe_for_binlog' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global log_statements_unsafe_for_binlog= TRUE;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'log_statements_unsafe_for_binlog= TRUE' at line 1
SET @@SESSION log_statements_unsafe_for_binlog= TRUE;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'log_statements_unsafe_for_binlog= TRUE' at line 1
SET @@global.log_statements_unsafe_for_binlog= @start_value;
