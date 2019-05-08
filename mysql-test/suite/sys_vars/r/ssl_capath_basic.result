'#---------------------BS_STVARS_046_01----------------------#'
SELECT COUNT(@@GLOBAL.ssl_capath);
COUNT(@@GLOBAL.ssl_capath)
0
0 Expected
'#---------------------BS_STVARS_046_02----------------------#'
SET @@GLOBAL.ssl_capath=1;
ERROR 42000: Incorrect argument type to variable 'ssl_capath'
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.ssl_capath);
COUNT(@@GLOBAL.ssl_capath)
0
0 Expected
'#---------------------BS_STVARS_046_03----------------------#'
SELECT @@GLOBAL.ssl_capath = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='ssl_capath';
@@GLOBAL.ssl_capath = VARIABLE_VALUE
NULL
1 Expected
SELECT COUNT(@@GLOBAL.ssl_capath);
COUNT(@@GLOBAL.ssl_capath)
0
0 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='ssl_capath';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_046_04----------------------#'
SELECT @@ssl_capath = @@GLOBAL.ssl_capath;
@@ssl_capath = @@GLOBAL.ssl_capath
NULL
1 Expected
'#---------------------BS_STVARS_046_05----------------------#'
SELECT COUNT(@@ssl_capath);
COUNT(@@ssl_capath)
0
0 Expected
SELECT COUNT(@@local.ssl_capath);
ERROR HY000: Variable 'ssl_capath' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.ssl_capath);
ERROR HY000: Variable 'ssl_capath' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.ssl_capath);
COUNT(@@GLOBAL.ssl_capath)
0
0 Expected
SELECT ssl_capath = @@SESSION.ssl_capath;
ERROR 42S22: Unknown column 'ssl_capath' in 'field list'
Expected error 'Readonly variable'
