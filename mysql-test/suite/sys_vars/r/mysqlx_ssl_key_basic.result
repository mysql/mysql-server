SELECT COUNT(@@GLOBAL.mysqlx_ssl_key);
COUNT(@@GLOBAL.mysqlx_ssl_key)
1
SET @@GLOBAL.mysqlx_ssl_key=1;
ERROR HY000: Variable 'mysqlx_ssl_key' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.mysqlx_ssl_key);
COUNT(@@GLOBAL.mysqlx_ssl_key)
1
SELECT @@GLOBAL.mysqlx_ssl_key = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='mysqlx_ssl_key';
@@GLOBAL.mysqlx_ssl_key = VARIABLE_VALUE
1
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='mysqlx_ssl_key';
COUNT(VARIABLE_VALUE)
1
SELECT @@mysqlx_ssl_key = @@GLOBAL.mysqlx_ssl_key;
@@mysqlx_ssl_key = @@GLOBAL.mysqlx_ssl_key
1
SELECT COUNT(@@mysqlx_ssl_key);
COUNT(@@mysqlx_ssl_key)
1
SELECT COUNT(@@local.mysqlx_ssl_key);
ERROR HY000: Variable 'mysqlx_ssl_key' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.mysqlx_ssl_key);
ERROR HY000: Variable 'mysqlx_ssl_key' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.mysqlx_ssl_key);
COUNT(@@GLOBAL.mysqlx_ssl_key)
1
SELECT mysqlx_ssl_key;
ERROR 42S22: Unknown column 'mysqlx_ssl_key' in 'field list'
Expected error 'Unknown column'
