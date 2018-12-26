SELECT COUNT(@@GLOBAL.mysqlx_ssl_cipher);
COUNT(@@GLOBAL.mysqlx_ssl_cipher)
0
SET @@GLOBAL.mysqlx_ssl_cipher=1;
ERROR HY000: Variable 'mysqlx_ssl_cipher' is a read only variable
Expected error 'Read only variable'
SELECT @@GLOBAL.mysqlx_ssl_cipher = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='mysqlx_ssl_cipher';
@@GLOBAL.mysqlx_ssl_cipher = VARIABLE_VALUE
NULL
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='mysqlx_ssl_cipher';
COUNT(VARIABLE_VALUE)
1
SELECT @@mysqlx_ssl_cipher = @@GLOBAL.mysqlx_ssl_cipher;
@@mysqlx_ssl_cipher = @@GLOBAL.mysqlx_ssl_cipher
NULL
SELECT COUNT(@@mysqlx_ssl_cipher);
COUNT(@@mysqlx_ssl_cipher)
0
SELECT COUNT(@@local.mysqlx_ssl_cipher);
ERROR HY000: Variable 'mysqlx_ssl_cipher' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.mysqlx_ssl_cipher);
ERROR HY000: Variable 'mysqlx_ssl_cipher' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.mysqlx_ssl_cipher);
COUNT(@@GLOBAL.mysqlx_ssl_cipher)
0
SELECT mysqlx_ssl_cipher = @@SESSION.mysqlx_ssl_cipher;
ERROR 42S22: Unknown column 'mysqlx_ssl_cipher' in 'field list'
Expected error 'Readonly variable'
