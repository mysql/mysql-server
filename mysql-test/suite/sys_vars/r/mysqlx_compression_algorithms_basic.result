SET @mysqlx_compression_algorithms_sav= @@global.mysqlx_compression_algorithms;
SET GLOBAL mysqlx_compression_algorithms="";
show variables like 'mysqlx_compression_algorithms';
Variable_name	Value
mysqlx_compression_algorithms	
SET @@global.mysqlx_compression_algorithms="LZ4_MESSAGE";
show variables like 'mysqlx_compression_algorithms';
Variable_name	Value
mysqlx_compression_algorithms	LZ4_MESSAGE
SET @@global.mysqlx_compression_algorithms="DEFLATE_STREAM,LZ4_MESSAGE";
show variables like 'mysqlx_compression_algorithms';
Variable_name	Value
mysqlx_compression_algorithms	DEFLATE_STREAM,LZ4_MESSAGE
SET mysqlx_compression_algorithms="LZ4_MESSAGE";
ERROR HY000: Variable 'mysqlx_compression_algorithms' is a GLOBAL variable and should be set with SET GLOBAL
SET @@local.mysqlx_compression_algorithms="LZ4_MESSAGE";
ERROR HY000: Variable 'mysqlx_compression_algorithms' is a GLOBAL variable and should be set with SET GLOBAL
SET @@mysqlx_compression_algorithms="LZ4_MESSAGE";
ERROR HY000: Variable 'mysqlx_compression_algorithms' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.mysqlx_compression_algorithms="LZ4_MESSAGE";
ERROR HY000: Variable 'mysqlx_compression_algorithms' is a GLOBAL variable and should be set with SET GLOBAL
SET SESSION mysqlx_compression_algorithms="LZ4_MESSAGE";
ERROR HY000: Variable 'mysqlx_compression_algorithms' is a GLOBAL variable and should be set with SET GLOBAL
SET GLOBAL mysqlx_compression_algorithms=1;
show variables like 'mysqlx_compression_algorithms';
Variable_name	Value
mysqlx_compression_algorithms	DEFLATE_STREAM
SET GLOBAL mysqlx_compression_algorithms=-848;
ERROR 42000: Variable 'mysqlx_compression_algorithms' can't be set to the value of '-848'
SET GLOBAL mysqlx_compression_algorithms=848;
ERROR 42000: Variable 'mysqlx_compression_algorithms' can't be set to the value of '848'
SET GLOBAL mysqlx_compression_algorithms='%45*#in';
ERROR 42000: Variable 'mysqlx_compression_algorithms' can't be set to the value of '%45*#in'
show variables like 'mysqlx_compression_algorithms';
Variable_name	Value
mysqlx_compression_algorithms	DEFLATE_STREAM
SET @@global.mysqlx_compression_algorithms= @mysqlx_compression_algorithms_sav;
