SET @mysqlx_enable_hello_notice_sav= @@global.mysqlx_enable_hello_notice;
SET GLOBAL mysqlx_enable_hello_notice=false;
show variables like 'mysqlx_enable_hello_notice';
Variable_name	Value
mysqlx_enable_hello_notice	OFF
SET @@global.mysqlx_enable_hello_notice=true;
show variables like 'mysqlx_enable_hello_notice';
Variable_name	Value
mysqlx_enable_hello_notice	ON
SET mysqlx_enable_hello_notice=true;
ERROR HY000: Variable 'mysqlx_enable_hello_notice' is a GLOBAL variable and should be set with SET GLOBAL
SET @@local.mysqlx_enable_hello_notice=true;
ERROR HY000: Variable 'mysqlx_enable_hello_notice' is a GLOBAL variable and should be set with SET GLOBAL
SET @@mysqlx_enable_hello_notice=true;
ERROR HY000: Variable 'mysqlx_enable_hello_notice' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.mysqlx_enable_hello_notice=true;
ERROR HY000: Variable 'mysqlx_enable_hello_notice' is a GLOBAL variable and should be set with SET GLOBAL
SET SESSION mysqlx_enable_hello_notice=true;
ERROR HY000: Variable 'mysqlx_enable_hello_notice' is a GLOBAL variable and should be set with SET GLOBAL
SET GLOBAL mysqlx_enable_hello_notice='%45*#in';
ERROR 42000: Variable 'mysqlx_enable_hello_notice' can't be set to the value of '%45*#in'
SELECT @@global.mysqlx_enable_hello_notice;
@@global.mysqlx_enable_hello_notice
1
SET @@global.mysqlx_enable_hello_notice = 4294967296;
ERROR 42000: Variable 'mysqlx_enable_hello_notice' can't be set to the value of '4294967296'
SELECT @@global.mysqlx_enable_hello_notice;
@@global.mysqlx_enable_hello_notice
1
SET @@global.mysqlx_enable_hello_notice = 10240022115;
ERROR 42000: Variable 'mysqlx_enable_hello_notice' can't be set to the value of '10240022115'
SELECT @@global.mysqlx_enable_hello_notice;
@@global.mysqlx_enable_hello_notice
1
SET @@global.mysqlx_enable_hello_notice = 10000.01;
ERROR 42000: Incorrect argument type to variable 'mysqlx_enable_hello_notice'
SELECT @@global.mysqlx_enable_hello_notice;
@@global.mysqlx_enable_hello_notice
1
SET @@global.mysqlx_enable_hello_notice = 42949672950;
ERROR 42000: Variable 'mysqlx_enable_hello_notice' can't be set to the value of '42949672950'
SELECT @@global.mysqlx_enable_hello_notice;
@@global.mysqlx_enable_hello_notice
1
SET @@global.mysqlx_enable_hello_notice= @mysqlx_enable_hello_notice_sav;
