SET @mysqlx_document_id_unique_prefix_sav= @@global.mysqlx_document_id_unique_prefix;
SET GLOBAL mysqlx_document_id_unique_prefix=7;
SET @@global.mysqlx_document_id_unique_prefix=7;
show variables like 'mysqlx_document_id_unique_prefix';
Variable_name	Value
mysqlx_document_id_unique_prefix	7
SET @@global.mysqlx_document_id_unique_prefix=3600;
show variables like 'mysqlx_document_id_unique_prefix';
Variable_name	Value
mysqlx_document_id_unique_prefix	3600
SET mysqlx_document_id_unique_prefix=7;
ERROR HY000: Variable 'mysqlx_document_id_unique_prefix' is a GLOBAL variable and should be set with SET GLOBAL
SET @@local.mysqlx_document_id_unique_prefix=7;
ERROR HY000: Variable 'mysqlx_document_id_unique_prefix' is a GLOBAL variable and should be set with SET GLOBAL
SET @@mysqlx_document_id_unique_prefix=7;
ERROR HY000: Variable 'mysqlx_document_id_unique_prefix' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.mysqlx_document_id_unique_prefix=7;
ERROR HY000: Variable 'mysqlx_document_id_unique_prefix' is a GLOBAL variable and should be set with SET GLOBAL
SET SESSION mysqlx_document_id_unique_prefix=7;
ERROR HY000: Variable 'mysqlx_document_id_unique_prefix' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global.mysqlx_document_id_unique_prefix=3601;
SET GLOBAL mysqlx_document_id_unique_prefix=0;
SET GLOBAL mysqlx_document_id_unique_prefix=-1;
Warnings:
Warning	1292	Truncated incorrect mysqlx_document_id_unique_prefix value: '-1'
SET GLOBAL mysqlx_document_id_unique_prefix='invaid%char';
ERROR 42000: Incorrect argument type to variable 'mysqlx_document_id_unique_prefix'
show variables like 'mysqlx_document_id_unique_prefix';
Variable_name	Value
mysqlx_document_id_unique_prefix	0
SET GLOBAL mysqlx_document_id_unique_prefix=65535;
show variables like 'mysqlx_document_id_unique_prefix';
Variable_name	Value
mysqlx_document_id_unique_prefix	65535
SET GLOBAL mysqlx_document_id_unique_prefix=65536;
Warnings:
Warning	1292	Truncated incorrect mysqlx_document_id_unique_prefix value: '65536'
show variables like 'mysqlx_document_id_unique_prefix';
Variable_name	Value
mysqlx_document_id_unique_prefix	65535
SET GLOBAL mysqlx_document_id_unique_prefix= @mysqlx_document_id_unique_prefix_sav;
