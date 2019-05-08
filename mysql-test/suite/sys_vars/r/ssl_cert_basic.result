'#---------------------BS_STVARS_047_01----------------------#'
SELECT COUNT(@@GLOBAL.ssl_cert);
COUNT(@@GLOBAL.ssl_cert)
1
1 Expected
'#---------------------BS_STVARS_047_02----------------------#'
SET @@GLOBAL.ssl_cert=1;
ERROR 42000: Incorrect argument type to variable 'ssl_cert'
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.ssl_cert);
COUNT(@@GLOBAL.ssl_cert)
1
1 Expected
'#---------------------BS_STVARS_047_03----------------------#'
SELECT @@GLOBAL.ssl_cert = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='ssl_cert';
@@GLOBAL.ssl_cert = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.ssl_cert);
COUNT(@@GLOBAL.ssl_cert)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='ssl_cert';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_047_04----------------------#'
SELECT @@ssl_cert = @@GLOBAL.ssl_cert;
@@ssl_cert = @@GLOBAL.ssl_cert
1
1 Expected
'#---------------------BS_STVARS_047_05----------------------#'
SELECT COUNT(@@ssl_cert);
COUNT(@@ssl_cert)
1
1 Expected
SELECT COUNT(@@local.ssl_cert);
ERROR HY000: Variable 'ssl_cert' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.ssl_cert);
ERROR HY000: Variable 'ssl_cert' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.ssl_cert);
COUNT(@@GLOBAL.ssl_cert)
1
1 Expected
SELECT ssl_cert = @@SESSION.ssl_cert;
ERROR 42S22: Unknown column 'ssl_cert' in 'field list'
Expected error 'Readonly variable'
