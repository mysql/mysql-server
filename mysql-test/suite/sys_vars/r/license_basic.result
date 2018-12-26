'#---------------------BS_STVARS_040_01----------------------#'
SELECT COUNT(@@GLOBAL.license);
COUNT(@@GLOBAL.license)
1
1 Expected
'#---------------------BS_STVARS_040_02----------------------#'
SET @@GLOBAL.license=1;
ERROR HY000: Variable 'license' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.license);
COUNT(@@GLOBAL.license)
1
1 Expected
'#---------------------BS_STVARS_040_03----------------------#'
SELECT @@GLOBAL.license = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='license';
@@GLOBAL.license = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.license);
COUNT(@@GLOBAL.license)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='license';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_040_04----------------------#'
SELECT @@license = @@GLOBAL.license;
@@license = @@GLOBAL.license
1
1 Expected
'#---------------------BS_STVARS_040_05----------------------#'
SELECT COUNT(@@license);
COUNT(@@license)
1
1 Expected
SELECT COUNT(@@local.license);
ERROR HY000: Variable 'license' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.license);
ERROR HY000: Variable 'license' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.license);
COUNT(@@GLOBAL.license)
1
1 Expected
SELECT license = @@SESSION.license;
ERROR 42S22: Unknown column 'license' in 'field list'
Expected error 'Readonly variable'
