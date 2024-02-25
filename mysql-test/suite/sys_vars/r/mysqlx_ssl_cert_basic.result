SELECT COUNT(@@GLOBAL.mysqlx_ssl_cert);
COUNT(@@GLOBAL.mysqlx_ssl_cert)
1
SET @@GLOBAL.mysqlx_ssl_cert=1;
ERROR HY000: Variable 'mysqlx_ssl_cert' is a read only variable
Expected error 'Read only variable'
SELECT @@GLOBAL.mysqlx_ssl_cert = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='mysqlx_ssl_cert';
@@GLOBAL.mysqlx_ssl_cert = VARIABLE_VALUE
1
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='mysqlx_ssl_cert';
COUNT(VARIABLE_VALUE)
1
SELECT @@mysqlx_ssl_cert = @@GLOBAL.mysqlx_ssl_cert;
@@mysqlx_ssl_cert = @@GLOBAL.mysqlx_ssl_cert
1
SELECT COUNT(@@mysqlx_ssl_cert);
COUNT(@@mysqlx_ssl_cert)
1
SELECT COUNT(@@local.mysqlx_ssl_cert);
ERROR HY000: Variable 'mysqlx_ssl_cert' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.mysqlx_ssl_cert);
ERROR HY000: Variable 'mysqlx_ssl_cert' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.mysqlx_ssl_cert);
COUNT(@@GLOBAL.mysqlx_ssl_cert)
1
SELECT mysqlx_ssl_cert;
ERROR 42S22: Unknown column 'mysqlx_ssl_cert' in 'field list'
Expected error 'Unknown column'
