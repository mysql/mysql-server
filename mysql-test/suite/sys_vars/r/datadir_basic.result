'#---------------------BS_STVARS_004_01----------------------#'
SELECT COUNT(@@GLOBAL.datadir);
COUNT(@@GLOBAL.datadir)
1
1 Expected
'#---------------------BS_STVARS_004_02----------------------#'
SET @@GLOBAL.datadir=1;
ERROR HY000: Variable 'datadir' is a read only variable
Expected error 'Read only variable'
SELECT COUNT(@@GLOBAL.datadir);
COUNT(@@GLOBAL.datadir)
1
1 Expected
'#---------------------BS_STVARS_004_03----------------------#'
SELECT @@GLOBAL.datadir = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='datadir';
@@GLOBAL.datadir = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.datadir);
COUNT(@@GLOBAL.datadir)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='datadir';
COUNT(VARIABLE_VALUE)
1
1 Expected
'#---------------------BS_STVARS_004_04----------------------#'
SELECT @@datadir = @@GLOBAL.datadir;
@@datadir = @@GLOBAL.datadir
1
1 Expected
'#---------------------BS_STVARS_004_05----------------------#'
SELECT COUNT(@@datadir);
COUNT(@@datadir)
1
1 Expected
SELECT COUNT(@@local.datadir);
ERROR HY000: Variable 'datadir' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.datadir);
ERROR HY000: Variable 'datadir' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.datadir);
COUNT(@@GLOBAL.datadir)
1
1 Expected
SELECT datadir = @@SESSION.datadir;
ERROR 42S22: Unknown column 'datadir' in 'field list'
Expected error 'Readonly variable'
