'#---------------------BS_STVARS_006_01----------------------#'
SELECT COUNT(@@GLOBAL.have_compress);
COUNT(@@GLOBAL.have_compress)
1
1 Expected
'#---------------------BS_STVARS_006_02----------------------#'
SET @@GLOBAL.have_compress=1;
ERROR HY000: Variable 'have_compress' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.have_compress);
COUNT(@@GLOBAL.have_compress)
1
1 Expected
'#---------------------BS_STVARS_006_03----------------------#'
SELECT @@GLOBAL.have_compress = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='have_compress';
@@GLOBAL.have_compress = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.have_compress);
COUNT(@@GLOBAL.have_compress)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='have_compress';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_006_04----------------------#'
SELECT @@have_compress = @@GLOBAL.have_compress;
@@have_compress = @@GLOBAL.have_compress
1
1 Expected
'#---------------------BS_STVARS_006_05----------------------#'
SELECT COUNT(@@have_compress);
COUNT(@@have_compress)
1
1 Expected
SELECT COUNT(@@local.have_compress);
ERROR HY000: Variable 'have_compress' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.have_compress);
ERROR HY000: Variable 'have_compress' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.have_compress);
COUNT(@@GLOBAL.have_compress)
1
1 Expected
SELECT have_compress;
ERROR 42S22: Unknown column 'have_compress' in 'field list'
Expected error 'Unknown column'
