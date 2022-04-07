'#---------------------BS_STVARS_013_01----------------------#'
SELECT COUNT(@@GLOBAL.have_openssl);
COUNT(@@GLOBAL.have_openssl)
1
1 Expected
'#---------------------BS_STVARS_013_02----------------------#'
SET @@GLOBAL.have_openssl=1;
ERROR HY000: Variable 'have_openssl' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.have_openssl);
COUNT(@@GLOBAL.have_openssl)
1
1 Expected
'#---------------------BS_STVARS_013_03----------------------#'
SELECT @@GLOBAL.have_openssl = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='have_openssl';
@@GLOBAL.have_openssl = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.have_openssl);
COUNT(@@GLOBAL.have_openssl)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='have_openssl';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_013_04----------------------#'
SELECT @@have_openssl = @@GLOBAL.have_openssl;
@@have_openssl = @@GLOBAL.have_openssl
1
1 Expected
'#---------------------BS_STVARS_013_05----------------------#'
SELECT COUNT(@@have_openssl);
COUNT(@@have_openssl)
1
1 Expected
SELECT COUNT(@@local.have_openssl);
ERROR HY000: Variable 'have_openssl' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.have_openssl);
ERROR HY000: Variable 'have_openssl' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.have_openssl);
COUNT(@@GLOBAL.have_openssl)
1
1 Expected
SELECT have_openssl = @@SESSION.have_openssl;
ERROR 42S22: Unknown column 'have_openssl' in 'field list'
Expected error 'Readonly variable'
