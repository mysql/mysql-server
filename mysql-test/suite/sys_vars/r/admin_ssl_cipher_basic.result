'#---------------------BS_STVARS_048_01----------------------#'
SELECT COUNT(@@GLOBAL.admin_ssl_cipher);
COUNT(@@GLOBAL.admin_ssl_cipher)
0
0 Expected
'#---------------------BS_STVARS_048_02----------------------#'
SET @@GLOBAL.admin_ssl_cipher=1;
ERROR 42000: Incorrect argument type to variable 'admin_ssl_cipher'
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.admin_ssl_cipher);
COUNT(@@GLOBAL.admin_ssl_cipher)
0
0 Expected
'#---------------------BS_STVARS_048_03----------------------#'
SELECT @@GLOBAL.admin_ssl_cipher = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='admin_ssl_cipher';
@@GLOBAL.admin_ssl_cipher = VARIABLE_VALUE
NULL
NULL Expected
SELECT COUNT(@@GLOBAL.admin_ssl_cipher);
COUNT(@@GLOBAL.admin_ssl_cipher)
0
0 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='admin_ssl_cipher';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_048_04----------------------#'
SELECT @@admin_ssl_cipher = @@GLOBAL.admin_ssl_cipher;
@@admin_ssl_cipher = @@GLOBAL.admin_ssl_cipher
NULL
NULL Expected
'#---------------------BS_STVARS_048_05----------------------#'
SELECT COUNT(@@admin_ssl_cipher);
COUNT(@@admin_ssl_cipher)
0
0 Expected
SELECT COUNT(@@local.admin_ssl_cipher);
ERROR HY000: Variable 'admin_ssl_cipher' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.admin_ssl_cipher);
ERROR HY000: Variable 'admin_ssl_cipher' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.admin_ssl_cipher);
COUNT(@@GLOBAL.admin_ssl_cipher)
0
0 Expected
SELECT admin_ssl_cipher;
ERROR 42S22: Unknown column 'admin_ssl_cipher' in 'field list'
Expected error 'Unknown column'
