SET @mysqlx_max_allowed_packet_sav= @@global.mysqlx_max_allowed_packet;
SET GLOBAL mysqlx_max_allowed_packet=5000000;
SET @@global.mysqlx_max_allowed_packet=7000000;
show variables like 'mysqlx_max_allowed_packet';
Variable_name	Value
mysqlx_max_allowed_packet	7000000
SET mysqlx_max_allowed_packet=5000000;
ERROR HY000: Variable 'mysqlx_max_allowed_packet' is a GLOBAL variable and should be set with SET GLOBAL
SET @@local.mysqlx_max_allowed_packet=5000000;
ERROR HY000: Variable 'mysqlx_max_allowed_packet' is a GLOBAL variable and should be set with SET GLOBAL
SET @@mysqlx_max_allowed_packet=5000000;
ERROR HY000: Variable 'mysqlx_max_allowed_packet' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.mysqlx_max_allowed_packet=5000000;
ERROR HY000: Variable 'mysqlx_max_allowed_packet' is a GLOBAL variable and should be set with SET GLOBAL
SET SESSION mysqlx_max_allowed_packet=5000000;
ERROR HY000: Variable 'mysqlx_max_allowed_packet' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global.mysqlx_max_allowed_packet=0;
Warnings:
Warning	1292	Truncated incorrect mysqlx_max_allowed_packet value: '0'
SET @@global.mysqlx_max_allowed_packet=-123;
Warnings:
Warning	1292	Truncated incorrect mysqlx_max_allowed_packet value: '-123'
SET @@global.mysqlx_max_allowed_packet='crash#$%';
ERROR 42000: Incorrect argument type to variable 'mysqlx_max_allowed_packet'
show variables like 'mysqlx_max_allowed_packet';
Variable_name	Value
mysqlx_max_allowed_packet	512
SET @@global.mysqlx_max_allowed_packet=1048576;
SET @@global.mysqlx_max_allowed_packet= @mysqlx_max_allowed_packet_sav;
