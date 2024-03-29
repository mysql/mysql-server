SET GLOBAL mysqlx_lz4_max_client_compression_level=0;
show variables like 'mysqlx_lz4_max_client_compression_level';
Variable_name	Value
mysqlx_lz4_max_client_compression_level	0
SET GLOBAL mysqlx_lz4_max_client_compression_level=1;
show variables like 'mysqlx_lz4_max_client_compression_level';
Variable_name	Value
mysqlx_lz4_max_client_compression_level	1
SET @@global.mysqlx_lz4_max_client_compression_level=2;
show variables like 'mysqlx_lz4_max_client_compression_level';
Variable_name	Value
mysqlx_lz4_max_client_compression_level	2
SET mysqlx_lz4_max_client_compression_level=4;
ERROR HY000: Variable 'mysqlx_lz4_max_client_compression_level' is a GLOBAL variable and should be set with SET GLOBAL
SET @@local.mysqlx_lz4_max_client_compression_level=4;
ERROR HY000: Variable 'mysqlx_lz4_max_client_compression_level' is a GLOBAL variable and should be set with SET GLOBAL
SET @@mysqlx_lz4_max_client_compression_level=4;
ERROR HY000: Variable 'mysqlx_lz4_max_client_compression_level' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.mysqlx_lz4_max_client_compression_level=4;
ERROR HY000: Variable 'mysqlx_lz4_max_client_compression_level' is a GLOBAL variable and should be set with SET GLOBAL
SET SESSION mysqlx_lz4_max_client_compression_level=4;
ERROR HY000: Variable 'mysqlx_lz4_max_client_compression_level' is a GLOBAL variable and should be set with SET GLOBAL
SET GLOBAL mysqlx_lz4_max_client_compression_level='5';
ERROR 42000: Incorrect argument type to variable 'mysqlx_lz4_max_client_compression_level'
SET GLOBAL mysqlx_lz4_max_client_compression_level=128;
ERROR 42000: Variable 'mysqlx_lz4_max_client_compression_level' can't be set to the value of '128'
SET GLOBAL mysqlx_lz4_max_client_compression_level=-1;
ERROR 42000: Variable 'mysqlx_lz4_max_client_compression_level' can't be set to the value of '-1'
SET GLOBAL mysqlx_lz4_max_client_compression_level=-128;
ERROR 42000: Variable 'mysqlx_lz4_max_client_compression_level' can't be set to the value of '-128'
SET @@global.mysqlx_lz4_max_client_compression_level= DEFAULT;
