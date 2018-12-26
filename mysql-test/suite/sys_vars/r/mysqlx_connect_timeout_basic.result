SET @mysqlx_connect_timeout_sav= @@global.mysqlx_connect_timeout;
SET GLOBAL mysqlx_connect_timeout=20;
SET @@global.mysqlx_connect_timeout=500;
show variables like 'mysqlx_connect_timeout';
Variable_name	Value
mysqlx_connect_timeout	500
SET mysqlx_connect_timeout=20;
ERROR HY000: Variable 'mysqlx_connect_timeout' is a GLOBAL variable and should be set with SET GLOBAL
SET @@local.mysqlx_connect_timeout=20;
ERROR HY000: Variable 'mysqlx_connect_timeout' is a GLOBAL variable and should be set with SET GLOBAL
SET @@mysqlx_connect_timeout=20;
ERROR HY000: Variable 'mysqlx_connect_timeout' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.mysqlx_connect_timeout=20;
ERROR HY000: Variable 'mysqlx_connect_timeout' is a GLOBAL variable and should be set with SET GLOBAL
SET SESSION mysqlx_connect_timeout=20;
ERROR HY000: Variable 'mysqlx_connect_timeout' is a GLOBAL variable and should be set with SET GLOBAL
SET GLOBAL mysqlx_connect_timeout=0;
Warnings:
Warning	1292	Truncated incorrect mysqlx_connect_timeout value: '0'
SET GLOBAL mysqlx_connect_timeout=1;
SET GLOBAL mysqlx_connect_timeout=-4686234;
Warnings:
Warning	1292	Truncated incorrect mysqlx_connect_timeout value: '-4686234'
SET GLOBAL mysqlx_connect_timeout='gduges7';
ERROR 42000: Incorrect argument type to variable 'mysqlx_connect_timeout'
show variables like 'mysqlx_connect_timeout';
Variable_name	Value
mysqlx_connect_timeout	1
SET GLOBAL mysqlx_connect_timeout=300;
SET @@global.mysqlx_connect_timeout= @mysqlx_connect_timeout_sav;
