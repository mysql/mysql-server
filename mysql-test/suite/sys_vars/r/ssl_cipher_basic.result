'#---------------------BS_STVARS_048_01----------------------#'
SELECT COUNT(@@GLOBAL.ssl_cipher);
COUNT(@@GLOBAL.ssl_cipher)
0
0 Expected
'#---------------------BS_STVARS_048_02----------------------#'
SET @@GLOBAL.ssl_cipher=1;
ERROR 42000: Incorrect argument type to variable 'ssl_cipher'
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.ssl_cipher);
COUNT(@@GLOBAL.ssl_cipher)
0
0 Expected
'#---------------------BS_STVARS_048_03----------------------#'
SELECT @@GLOBAL.ssl_cipher = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='ssl_cipher';
@@GLOBAL.ssl_cipher = VARIABLE_VALUE
NULL
1 Expected
SELECT COUNT(@@GLOBAL.ssl_cipher);
COUNT(@@GLOBAL.ssl_cipher)
0
0 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='ssl_cipher';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_048_04----------------------#'
SELECT @@ssl_cipher = @@GLOBAL.ssl_cipher;
@@ssl_cipher = @@GLOBAL.ssl_cipher
NULL
1 Expected
'#---------------------BS_STVARS_048_05----------------------#'
SELECT COUNT(@@ssl_cipher);
COUNT(@@ssl_cipher)
0
0 Expected
SELECT COUNT(@@local.ssl_cipher);
ERROR HY000: Variable 'ssl_cipher' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.ssl_cipher);
ERROR HY000: Variable 'ssl_cipher' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.ssl_cipher);
COUNT(@@GLOBAL.ssl_cipher)
0
0 Expected
SELECT ssl_cipher;
ERROR 42S22: Unknown column 'ssl_cipher' in 'field list'
Expected error 'Unknown column'
