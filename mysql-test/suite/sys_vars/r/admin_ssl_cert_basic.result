'#---------------------BS_STVARS_045_01----------------------#'
SELECT COUNT(@@GLOBAL.admin_ssl_cert);
COUNT(@@GLOBAL.admin_ssl_cert)
0
0 Expected
'#---------------------BS_STVARS_045_02----------------------#'
SET @@GLOBAL.admin_ssl_cert=1;
ERROR 42000: Incorrect argument type to variable 'admin_ssl_cert'
Expected error 'Incorrect argument type'
SELECT COUNT(@@GLOBAL.admin_ssl_cert);
COUNT(@@GLOBAL.admin_ssl_cert)
0
0 Expected
'#---------------------BS_STVARS_045_03----------------------#'
SELECT VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='admin_ssl_cert';
VARIABLE_VALUE

NULL Expected
SELECT COUNT(@@GLOBAL.admin_ssl_cert);
COUNT(@@GLOBAL.admin_ssl_cert)
0
0 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='admin_ssl_cert';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_045_04----------------------#'
SELECT @@admin_ssl_cert = @@GLOBAL.admin_ssl_cert;
@@admin_ssl_cert = @@GLOBAL.admin_ssl_cert
NULL
NULL Expected
'#---------------------BS_STVARS_045_05----------------------#'
SELECT COUNT(@@admin_ssl_cert);
COUNT(@@admin_ssl_cert)
0
0 Expected
SELECT COUNT(@@local.admin_ssl_cert);
ERROR HY000: Variable 'admin_ssl_cert' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.admin_ssl_cert);
ERROR HY000: Variable 'admin_ssl_cert' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.admin_ssl_cert);
COUNT(@@GLOBAL.admin_ssl_cert)
0
0 Expected
SET admin_ssl_cert = @@GLOBAL.admin_ssl_cert;
ERROR HY000: Variable 'admin_ssl_cert' is a GLOBAL variable and should be set with SET GLOBAL
Expected error 'Variable is a GLOBAL variable'
