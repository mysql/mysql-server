'#---------------------BS_STVARS_045_01----------------------#'
SELECT COUNT(@@GLOBAL.admin_tls_ciphersuites);
COUNT(@@GLOBAL.admin_tls_ciphersuites)
0
0 Expected
'#---------------------BS_STVARS_045_02----------------------#'
SET @@GLOBAL.admin_tls_ciphersuites=1;
ERROR 42000: Incorrect argument type to variable 'admin_tls_ciphersuites'
Expected error 'Incorrect argument type'
SELECT COUNT(@@GLOBAL.admin_tls_ciphersuites);
COUNT(@@GLOBAL.admin_tls_ciphersuites)
0
0 Expected
'#---------------------BS_STVARS_045_03----------------------#'
SELECT VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='admin_tls_ciphersuites';
VARIABLE_VALUE

NULL Expected
SELECT COUNT(@@GLOBAL.admin_tls_ciphersuites);
COUNT(@@GLOBAL.admin_tls_ciphersuites)
0
0 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='admin_tls_ciphersuites';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_045_04----------------------#'
SELECT @@admin_tls_ciphersuites = @@GLOBAL.admin_tls_ciphersuites;
@@admin_tls_ciphersuites = @@GLOBAL.admin_tls_ciphersuites
NULL
NULL Expected
'#---------------------BS_STVARS_045_05----------------------#'
SELECT COUNT(@@admin_tls_ciphersuites);
COUNT(@@admin_tls_ciphersuites)
0
0 Expected
SELECT COUNT(@@local.admin_tls_ciphersuites);
ERROR HY000: Variable 'admin_tls_ciphersuites' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.admin_tls_ciphersuites);
ERROR HY000: Variable 'admin_tls_ciphersuites' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.admin_tls_ciphersuites);
COUNT(@@GLOBAL.admin_tls_ciphersuites)
0
0 Expected
SET admin_tls_ciphersuites = @@GLOBAL.admin_tls_ciphersuites;
ERROR HY000: Variable 'admin_tls_ciphersuites' is a GLOBAL variable and should be set with SET GLOBAL
Expected error 'Variable is a GLOBAL variable'
