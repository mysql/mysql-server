SELECT COUNT(@@GLOBAL.mysqlx_ssl_ca);
COUNT(@@GLOBAL.mysqlx_ssl_ca)
1
SET @@GLOBAL.mysqlx_ssl_ca=1;
ERROR HY000: Variable 'mysqlx_ssl_ca' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.mysqlx_ssl_ca);
COUNT(@@GLOBAL.mysqlx_ssl_ca)
1
SELECT @@GLOBAL.mysqlx_ssl_ca = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='mysqlx_ssl_ca';
@@GLOBAL.mysqlx_ssl_ca = VARIABLE_VALUE
1
SELECT COUNT(@@GLOBAL.mysqlx_ssl_ca);
COUNT(@@GLOBAL.mysqlx_ssl_ca)
1
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='mysqlx_ssl_ca';
COUNT(VARIABLE_VALUE)
1
SELECT @@mysqlx_ssl_ca = @@GLOBAL.mysqlx_ssl_ca;
@@mysqlx_ssl_ca = @@GLOBAL.mysqlx_ssl_ca
1
SELECT COUNT(@@mysqlx_ssl_ca);
COUNT(@@mysqlx_ssl_ca)
1
SELECT COUNT(@@local.mysqlx_ssl_ca);
ERROR HY000: Variable 'mysqlx_ssl_ca' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.mysqlx_ssl_ca);
ERROR HY000: Variable 'mysqlx_ssl_ca' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.mysqlx_ssl_ca);
COUNT(@@GLOBAL.mysqlx_ssl_ca)
1
SELECT mysqlx_ssl_ca;
ERROR 42S22: Unknown column 'mysqlx_ssl_ca' in 'field list'
Expected error 'Unknown column'
