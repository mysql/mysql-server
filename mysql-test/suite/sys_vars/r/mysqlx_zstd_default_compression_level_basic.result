SET GLOBAL mysqlx_zstd_default_compression_level=1;
SET @@global.mysqlx_zstd_default_compression_level=2;
SET @@global.mysqlx_zstd_default_compression_level=3;
show variables like 'mysqlx_zstd_default_compression_level';
Variable_name	Value
mysqlx_zstd_default_compression_level	3
SET GLOBAL mysqlx_zstd_default_compression_level=-1;
SET @@global.mysqlx_zstd_default_compression_level=-2;
SET mysqlx_zstd_default_compression_level=4;
ERROR HY000: Variable 'mysqlx_zstd_default_compression_level' is a GLOBAL variable and should be set with SET GLOBAL
SET @@local.mysqlx_zstd_default_compression_level=4;
ERROR HY000: Variable 'mysqlx_zstd_default_compression_level' is a GLOBAL variable and should be set with SET GLOBAL
SET @@mysqlx_zstd_default_compression_level=4;
ERROR HY000: Variable 'mysqlx_zstd_default_compression_level' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.mysqlx_zstd_default_compression_level=4;
ERROR HY000: Variable 'mysqlx_zstd_default_compression_level' is a GLOBAL variable and should be set with SET GLOBAL
SET SESSION mysqlx_zstd_default_compression_level=4;
ERROR HY000: Variable 'mysqlx_zstd_default_compression_level' is a GLOBAL variable and should be set with SET GLOBAL
SET GLOBAL mysqlx_zstd_default_compression_level='5';
ERROR 42000: Incorrect argument type to variable 'mysqlx_zstd_default_compression_level'
SET GLOBAL mysqlx_zstd_default_compression_level=0;
ERROR 42000: Variable 'mysqlx_zstd_default_compression_level' can't be set to the value of '0'
SET GLOBAL mysqlx_zstd_default_compression_level=128;
ERROR 42000: Variable 'mysqlx_zstd_default_compression_level' can't be set to the value of '128'
SET GLOBAL mysqlx_zstd_default_compression_level=-150000;
ERROR 42000: Variable 'mysqlx_zstd_default_compression_level' can't be set to the value of '-150000'
SET @@global.mysqlx_zstd_default_compression_level= DEFAULT;
