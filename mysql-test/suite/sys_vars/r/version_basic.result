'#---------------------BS_STVARS_053_01----------------------#'
SELECT COUNT(@@GLOBAL.version);
COUNT(@@GLOBAL.version)
1
1 Expected
'#---------------------BS_STVARS_053_02----------------------#'
SET @@GLOBAL.version=1;
ERROR HY000: Variable 'version' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.version);
COUNT(@@GLOBAL.version)
1
1 Expected
'#---------------------BS_STVARS_053_03----------------------#'
SELECT @@GLOBAL.version = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='version';
@@GLOBAL.version = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.version);
COUNT(@@GLOBAL.version)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='version';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_053_04----------------------#'
SELECT @@version = @@GLOBAL.version;
@@version = @@GLOBAL.version
1
1 Expected
'#---------------------BS_STVARS_053_05----------------------#'
SELECT COUNT(@@version);
COUNT(@@version)
1
1 Expected
SELECT COUNT(@@local.version);
ERROR HY000: Variable 'version' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.version);
ERROR HY000: Variable 'version' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.version);
COUNT(@@GLOBAL.version)
1
1 Expected
SELECT version = @@SESSION.version;
ERROR 42S22: Unknown column 'version' in 'field list'
Expected error 'Readonly variable'
