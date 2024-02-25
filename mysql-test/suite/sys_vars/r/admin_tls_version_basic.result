'#---------------------BS_STVARS_045_01----------------------#'
SELECT COUNT(@@GLOBAL.admin_tls_version);
COUNT(@@GLOBAL.admin_tls_version)
1
1 Expected
'#---------------------BS_STVARS_045_02----------------------#'
SET @@GLOBAL.admin_tls_version=1;
ERROR 42000: Incorrect argument type to variable 'admin_tls_version'
Expected error 'Incorrect argument type'
SELECT COUNT(@@GLOBAL.admin_tls_version);
COUNT(@@GLOBAL.admin_tls_version)
1
1 Expected
'#---------------------BS_STVARS_045_03----------------------#'
SELECT @@GLOBAL.admin_tls_version = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='admin_tls_version';
@@GLOBAL.admin_tls_version = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.admin_tls_version);
COUNT(@@GLOBAL.admin_tls_version)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='admin_tls_version';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_045_04----------------------#'
SELECT @@admin_tls_version = @@GLOBAL.admin_tls_version;
@@admin_tls_version = @@GLOBAL.admin_tls_version
1
1 Expected
'#---------------------BS_STVARS_045_05----------------------#'
SELECT COUNT(@@admin_tls_version);
COUNT(@@admin_tls_version)
1
1 Expected
SELECT COUNT(@@local.admin_tls_version);
ERROR HY000: Variable 'admin_tls_version' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.admin_tls_version);
ERROR HY000: Variable 'admin_tls_version' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.admin_tls_version);
COUNT(@@GLOBAL.admin_tls_version)
1
1 Expected
SET admin_tls_version = @@GLOBAL.admin_tls_version;
ERROR HY000: Variable 'admin_tls_version' is a GLOBAL variable and should be set with SET GLOBAL
Expected error 'Variable is a GLOBAL variable'
