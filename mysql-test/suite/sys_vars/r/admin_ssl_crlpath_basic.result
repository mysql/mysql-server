'#---------------------BS_STVARS_045_01----------------------#'
SELECT COUNT(@@GLOBAL.admin_ssl_crlpath);
COUNT(@@GLOBAL.admin_ssl_crlpath)
0
0 Expected
'#---------------------BS_STVARS_045_02----------------------#'
SET @@GLOBAL.admin_ssl_crlpath=1;
ERROR 42000: Incorrect argument type to variable 'admin_ssl_crlpath'
Expected error 'Incorrect argument type'
SELECT COUNT(@@GLOBAL.admin_ssl_crlpath);
COUNT(@@GLOBAL.admin_ssl_crlpath)
0
0 Expected
'#---------------------BS_STVARS_045_03----------------------#'
SELECT VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='admin_ssl_crlpath';
VARIABLE_VALUE

NULL Expected
SELECT COUNT(@@GLOBAL.admin_ssl_crlpath);
COUNT(@@GLOBAL.admin_ssl_crlpath)
0
0 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='admin_ssl_crlpath';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_045_04----------------------#'
SELECT @@admin_ssl_crlpath = @@GLOBAL.admin_ssl_crlpath;
@@admin_ssl_crlpath = @@GLOBAL.admin_ssl_crlpath
NULL
NULL Expected
'#---------------------BS_STVARS_045_05----------------------#'
SELECT COUNT(@@admin_ssl_crlpath);
COUNT(@@admin_ssl_crlpath)
0
0 Expected
SELECT COUNT(@@local.admin_ssl_crlpath);
ERROR HY000: Variable 'admin_ssl_crlpath' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.admin_ssl_crlpath);
ERROR HY000: Variable 'admin_ssl_crlpath' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.admin_ssl_crlpath);
COUNT(@@GLOBAL.admin_ssl_crlpath)
0
0 Expected
SET admin_ssl_crlpath = @@GLOBAL.admin_ssl_crlpath;
ERROR HY000: Variable 'admin_ssl_crlpath' is a GLOBAL variable and should be set with SET GLOBAL
Expected error 'Variable is a GLOBAL variable'
