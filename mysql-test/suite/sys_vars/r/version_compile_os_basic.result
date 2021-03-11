'#---------------------BS_STVARS_056_01----------------------#'
SELECT COUNT(@@GLOBAL.version_compile_os);
COUNT(@@GLOBAL.version_compile_os)
1
1 Expected
'#---------------------BS_STVARS_056_02----------------------#'
SET @@GLOBAL.version_compile_os=1;
ERROR HY000: Variable 'version_compile_os' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.version_compile_os);
COUNT(@@GLOBAL.version_compile_os)
1
1 Expected
'#---------------------BS_STVARS_056_03----------------------#'
SELECT @@GLOBAL.version_compile_os = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='version_compile_os';
@@GLOBAL.version_compile_os = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.version_compile_os);
COUNT(@@GLOBAL.version_compile_os)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='version_compile_os';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_056_04----------------------#'
SELECT @@version_compile_os = @@GLOBAL.version_compile_os;
@@version_compile_os = @@GLOBAL.version_compile_os
1
1 Expected
'#---------------------BS_STVARS_056_05----------------------#'
SELECT COUNT(@@version_compile_os);
COUNT(@@version_compile_os)
1
1 Expected
SELECT COUNT(@@local.version_compile_os);
ERROR HY000: Variable 'version_compile_os' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.version_compile_os);
ERROR HY000: Variable 'version_compile_os' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.version_compile_os);
COUNT(@@GLOBAL.version_compile_os)
1
1 Expected
SELECT version_compile_os;
ERROR 42S22: Unknown column 'version_compile_os' in 'field list'
Expected error 'Unknown column'
