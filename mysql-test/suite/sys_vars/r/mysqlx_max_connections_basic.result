SET @mysqlx_max_connections_sav= @@global.mysqlx_max_connections;
SET GLOBAL mysqlx_max_connections=150;
show variables like 'mysqlx_max_connections';
Variable_name	Value
mysqlx_max_connections	150
SET @@global.mysqlx_max_connections=500;
show variables like 'mysqlx_max_connections';
Variable_name	Value
mysqlx_max_connections	500
SET mysqlx_max_connections=150;
ERROR HY000: Variable 'mysqlx_max_connections' is a GLOBAL variable and should be set with SET GLOBAL
SET @@local.mysqlx_max_connections=150;
ERROR HY000: Variable 'mysqlx_max_connections' is a GLOBAL variable and should be set with SET GLOBAL
SET @@mysqlx_max_connections=150;
ERROR HY000: Variable 'mysqlx_max_connections' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.mysqlx_max_connections=150;
ERROR HY000: Variable 'mysqlx_max_connections' is a GLOBAL variable and should be set with SET GLOBAL
SET SESSION mysqlx_max_connections=150;
ERROR HY000: Variable 'mysqlx_max_connections' is a GLOBAL variable and should be set with SET GLOBAL
SET GLOBAL mysqlx_max_connections=0;
Warnings:
Warning	1292	Truncated incorrect mysqlx_max_connections value: '0'
SET GLOBAL mysqlx_max_connections=-848;
Warnings:
Warning	1292	Truncated incorrect mysqlx_max_connections value: '-848'
SET GLOBAL mysqlx_max_connections='%45*#in';
ERROR 42000: Incorrect argument type to variable 'mysqlx_max_connections'
show variables like 'mysqlx_max_connections';
Variable_name	Value
mysqlx_max_connections	1
SET @@global.mysqlx_max_connections=151;
SET @@global.mysqlx_max_connections= @mysqlx_max_connections_sav;
