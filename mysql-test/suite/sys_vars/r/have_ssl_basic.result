'#---------------------BS_STVARS_017_01----------------------#'
SELECT COUNT(@@GLOBAL.have_ssl);
COUNT(@@GLOBAL.have_ssl)
1
1 Expected
'#---------------------BS_STVARS_017_02----------------------#'
SET @@GLOBAL.have_ssl=1;
ERROR HY000: Variable 'have_ssl' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.have_ssl);
COUNT(@@GLOBAL.have_ssl)
1
1 Expected
'#---------------------BS_STVARS_017_03----------------------#'
SELECT @@GLOBAL.have_ssl = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='have_ssl';
@@GLOBAL.have_ssl = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.have_ssl);
COUNT(@@GLOBAL.have_ssl)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='have_ssl';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_017_04----------------------#'
SELECT @@have_ssl = @@GLOBAL.have_ssl;
@@have_ssl = @@GLOBAL.have_ssl
1
1 Expected
'#---------------------BS_STVARS_017_05----------------------#'
SELECT COUNT(@@have_ssl);
COUNT(@@have_ssl)
1
1 Expected
SELECT COUNT(@@local.have_ssl);
ERROR HY000: Variable 'have_ssl' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.have_ssl);
ERROR HY000: Variable 'have_ssl' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.have_ssl);
COUNT(@@GLOBAL.have_ssl)
1
1 Expected
SELECT have_ssl = @@SESSION.have_ssl;
ERROR 42S22: Unknown column 'have_ssl' in 'field list'
Expected error 'Readonly variable'
