SET @start_value= @@global.log_queries_not_using_indexes;
SET @@global.log_queries_not_using_indexes= DEFAULT;
SELECT @@global.log_queries_not_using_indexes;
@@global.log_queries_not_using_indexes
0
SET @@global.log_queries_not_using_indexes= TRUE;
SELECT @@global.log_queries_not_using_indexes;
@@global.log_queries_not_using_indexes
1
SET @@global.log_queries_not_using_indexes= true;
SELECT @@global.log_queries_not_using_indexes;
@@global.log_queries_not_using_indexes
1
SET @@global.log_queries_not_using_indexes= 0;
SELECT @@global.log_queries_not_using_indexes;
@@global.log_queries_not_using_indexes
0
SET @@global.log_queries_not_using_indexes= 1;
SELECT @@global.log_queries_not_using_indexes;
@@global.log_queries_not_using_indexes
1
SET @goodvar= TRUE;
SET @@global.log_queries_not_using_indexes= @goodvar;
SELECT @@global.log_queries_not_using_indexes;
@@global.log_queries_not_using_indexes
1
SET GLOBAL log_queries_not_using_indexes= DEFAULT;
SELECT @@global.log_queries_not_using_indexes;
@@global.log_queries_not_using_indexes
0
SET GLOBAL log_queries_not_using_indexes= ON;
SELECT @@global.log_queries_not_using_indexes;
@@global.log_queries_not_using_indexes
1
SET GLOBAL log_queries_not_using_indexes= OFF;
SELECT @@global.log_queries_not_using_indexes;
@@global.log_queries_not_using_indexes
0
SET GLOBAL log_queries_not_using_indexes= -0;
SELECT @@global.log_queries_not_using_indexes;
@@global.log_queries_not_using_indexes
0
SET GLOBAL log_queries_not_using_indexes= 0.00;
ERROR 42000: Incorrect argument type to variable 'log_queries_not_using_indexes'
SET GLOBAL log_queries_not_using_indexes= +0;
SELECT @@global.log_queries_not_using_indexes;
@@global.log_queries_not_using_indexes
0
SET @@global.log_queries_not_using_indexes= 'DEFAULT';
ERROR 42000: Variable 'log_queries_not_using_indexes' can't be set to the value of 'DEFAULT'
SET @@global.log_queries_not_using_indexes= 'true';
ERROR 42000: Variable 'log_queries_not_using_indexes' can't be set to the value of 'true'
SET @@global.log_queries_not_using_indexes= BLABLA;
ERROR 42000: Variable 'log_queries_not_using_indexes' can't be set to the value of 'BLABLA'
SET @@global.log_queries_not_using_indexes= 25;
ERROR 42000: Variable 'log_queries_not_using_indexes' can't be set to the value of '25'
SET GLOBAL log_queries_not_using_indexes= -1;
ERROR 42000: Variable 'log_queries_not_using_indexes' can't be set to the value of '-1'
SET @badvar= 'true';
SET @@global.log_queries_not_using_indexes= @badvar;
ERROR 42000: Variable 'log_queries_not_using_indexes' can't be set to the value of 'true'
SET GLOBAL log_queries_not_using_indexes= 'DEFAULT';
ERROR 42000: Variable 'log_queries_not_using_indexes' can't be set to the value of 'DEFAULT'
SET log_queries_not_using_indexes= TRUE;
ERROR HY000: Variable 'log_queries_not_using_indexes' is a GLOBAL variable and should be set with SET GLOBAL
SET SESSION log_queries_not_using_indexes= TRUE;
ERROR HY000: Variable 'log_queries_not_using_indexes' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.log_queries_not_using_indexes= TRUE;
ERROR HY000: Variable 'log_queries_not_using_indexes' is a GLOBAL variable and should be set with SET GLOBAL
SET LOCAL log_queries_not_using_indexes= TRUE;
ERROR HY000: Variable 'log_queries_not_using_indexes' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global log_queries_not_using_indexes= TRUE;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'log_queries_not_using_indexes= TRUE' at line 1
SET @@SESSION log_queries_not_using_indexes= TRUE;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'log_queries_not_using_indexes= TRUE' at line 1
SET @@global.log_queries_not_using_indexes= @start_value;
