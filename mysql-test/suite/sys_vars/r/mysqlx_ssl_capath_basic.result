SELECT COUNT(@@GLOBAL.mysqlx_ssl_capath);
COUNT(@@GLOBAL.mysqlx_ssl_capath)
0
0 Expected
SET @@GLOBAL.mysqlx_ssl_capath=1;
ERROR HY000: Variable 'mysqlx_ssl_capath' is a read only variable
Expected error 'Read only variable'
SELECT @@GLOBAL.mysqlx_ssl_capath = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='mysqlx_ssl_capath';
@@GLOBAL.mysqlx_ssl_capath = VARIABLE_VALUE
NULL
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='mysqlx_ssl_capath';
COUNT(VARIABLE_VALUE)
1
SELECT @@mysqlx_ssl_capath = @@GLOBAL.mysqlx_ssl_capath;
@@mysqlx_ssl_capath = @@GLOBAL.mysqlx_ssl_capath
NULL
SELECT COUNT(@@mysqlx_ssl_capath);
COUNT(@@mysqlx_ssl_capath)
0
0 Expected
SELECT COUNT(@@local.mysqlx_ssl_capath);
ERROR HY000: Variable 'mysqlx_ssl_capath' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.mysqlx_ssl_capath);
ERROR HY000: Variable 'mysqlx_ssl_capath' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.mysqlx_ssl_capath);
COUNT(@@GLOBAL.mysqlx_ssl_capath)
0
0 Expected
SELECT mysqlx_ssl_capath = @@SESSION.mysqlx_ssl_capath;
ERROR 42S22: Unknown column 'mysqlx_ssl_capath' in 'field list'
Expected error 'Readonly variable'
